/**
*   usage : 1. provide solver for Nurse Rostering Problem.
*
*   note :  1. [optimizable] Solver has virtual function.
*           2. set algorithm arguments in run().
*           3. use a priority queue to manage available nurse when assigning?
*           v4. record 8 day which the first day is last day of last week to unify
*              the succession judgment.
*           5. [optimizable] put evaluateObjValue() in initAssign() or call it in init() ?
*           6. [optimizable] evaluateConsecutiveDay() and evaluateConsecutiveDayOff() can
*               be put together and not consider if there is an assignment. but there is
*               a difficult problem with consecutive state on the beginning of the week.
*           7. [optimizable] make Weekday not related to Consecutive count from 0 to save space.
*           8. [optimizable] make shiftID count from 1 and make Shift::ID_NONE 0,
*               this will leave out isWorking() in isValidSuccesion().
*               also, add a slot after Sun in assign with shift ID_NONE to
*               leave out (weekday >= Weekday::Sun) in isValidPrior()
*           9. timeout may overflow in POSIX system since CLOCKS_PER_SEC is 1000000 which
*               makes clock_t can only count to about 4000 seconds.
*/

#ifndef TABUSOLVER_H
#define TABUSOLVER_H


#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>

#include "DebugFlag.h"
#include "NurseRostering.h"
#include "utility.h"


class NurseRostering::TabuSolver : public NurseRostering::Solver
{
public:
    static const std::string Name;

    virtual void init();
    virtual void solve();
    virtual History genHistory() const
    {
        return Solution( *this, optima.assign ).genHistory();
    }

    // initialize data about nurse-skill relation
    void initAssistData();  // initialize nurseWithSkill, nurseNumOfSkill
    TabuSolver( const NurseRostering &input, clock_t startTime = clock() );
    virtual ~TabuSolver() {}

private:
    // NurseWithSkill[skill][skillNum-1] is a set of nurses 
    // who have that skill and have skillNum skills in total
    typedef std::vector< std::vector<std::vector<NurseID> > > NurseWithSkill;

    class Solution
    {
    public:
        bool genInitAssign();   // assign must be default value( call resetAssign() )
        bool genInitAssign_BranchAndCut();
        void resetAssign();     // reset all shift to Shift::ID_NONE
        void evaluateObjValue();    // using assist data structure
        bool repair( const Timer &timer );  // make infeasible solution feasible

        // iteratively run local search and perturb
        void iterativeLocalSearch( const Timer &timer, Output &optima );
        // try add shift until there is no improvement , then try change shift,
        // then try remove shift, then try add shift again. if all of them
        // can't improve or time is out, return.
        void localSearch( const Timer &timer, Output &optima );
        void localSearchOnConsecutiveBorder( const Timer &timer, Output &optima );
        // randomly select add, change or remove shift until timeout
        void randomWalk( const Timer &timer, Output &optima );

        const Assign& getAssign() const { return assign; }
        // shift must not be none shift
        bool isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const;
        bool isValidPrior( NurseID nurse, ShiftID shift, int weekday ) const;

        // check if the result of incremental update, evaluate and checkObjValue is the same
        bool checkIncrementalUpdate();

        Solution( const TabuSolver &solver );
        Solution( const TabuSolver &solver, const Assign &assign );
        // there must not be self assignment and assign must be build from same problem
        void rebuildAssistData( const Assign &assign );
        Output genOutput() const
        {
            return Output( objValue, assign );
        }
        History genHistory() const; // history for next week

    private:
        // available nurses for a single assignment
        //==========================================
        // an   :   available nurse
        // ants :   available nurse this shift
        // antd :   available nurse this day
        // 
        //        i       k|      m|       
        //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // an| | |x| | | |y| | | |z| | | |
        //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //                 |       |       
        //               ants     antd
        //
        //  if an[i] is not available for this shift:
        //      an[i] <-> an[k];
        //      --ants;
        //  if an[i] is not available for this day:
        //      an[i] <-> an[k];
        //      an[k] <-> an[m];
        //      --ants;
        //      --antd;
        //==========================================
        class AvailableNurses
        {
        public:
            // reset flags of available nurses
            // this method should be called before any other invoking
            void setEnvironment( int weekday, SkillID skill );
            // reset validNurseNum_CurShift
            // this method should be called before getNurse()
            void setShift( ShiftID shift );

            // always get an available nurse and update validation information
            NurseID getNurse();

            AvailableNurses( const Solution &s ) :sln( s ), nurseWithSkill( s.solver.nurseWithSkill ) {}
        private:
            const Solution &sln;
            NurseWithSkill nurseWithSkill;

            int weekday;
            ShiftID shift;
            SkillID skill;
            unsigned minSkillNum;
            std::vector<int> validNurseNum_CurShift;
            std::vector<int> validNurseNum_CurDay;

        private:    // forbidden operators
            AvailableNurses& operator=(const AvailableNurses &) { return *this; }
        };

        // consecutive information for a nurse
        //==========================================
        // this is an demo for switching consecutive working day and day off.
        // consecutive assignments use the same method.
        // besides, there is 1 slot for history and update towards left side 
        // should test if weekday is greater or equal to Weekday::HIS which is 0.
        // 
        //    E L O L E E E
        //   +-+-+-+-+-+-+-+
        //   |0|1|2|3|4|4|4|  high
        //   |0|1|2|3|6|6|6|  low
        //   +-+-+-+-+-+-+-+        +-+-+-+-+-+-+-+
        //      |       |  E->O     |0|1|2|3|4|5|6|
        //      | L->O  +---------->|0|1|2|3|4|5|6|
        //      |                   +-+-+-+-+-+-+-+
        //      |  +-+-+-+-+-+-+-+
        //      +->|0|1|1|3|3|3|6|
        //         |0|2|2|5|5|5|6| 
        //         +-+-+-+-+-+-+-+
        //
        //==========================================
        class Consecutive
        {
        public:
            Consecutive() {}
            Consecutive( const History &his, NurseID nurse )
            {
                if (Assign::isWorking( his.lastShifts[nurse] )) {
                    std::fill( dayLow, dayLow + Weekday::SIZE, Weekday::Mon );
                    std::fill( dayHigh, dayHigh + Weekday::SIZE, Weekday::Sun );
                    std::fill( shiftLow, shiftLow + Weekday::SIZE, Weekday::Mon );
                    std::fill( shiftHigh, shiftHigh + Weekday::SIZE, Weekday::Sun );
                    dayHigh[Weekday::HIS] = Weekday::HIS;
                    dayLow[Weekday::HIS] = 1 - his.consecutiveDayNums[nurse];
                    shiftHigh[Weekday::HIS] = Weekday::HIS;
                    shiftLow[Weekday::HIS] = 1 - his.consecutiveShiftNums[nurse];
                } else {    // day off
                    std::fill( dayLow, dayLow + Weekday::SIZE, 1 - his.consecutiveDayoffNums[nurse] );
                    std::fill( dayHigh, dayHigh + Weekday::SIZE, Weekday::Sun );
                    std::fill( shiftLow, shiftLow + Weekday::SIZE, 1 - his.consecutiveDayoffNums[nurse] );
                    std::fill( shiftHigh, shiftHigh + Weekday::SIZE, Weekday::Sun );
                }
            }
            Consecutive( const Consecutive &c )
            {
                memcpy( dayHigh, c.dayHigh, sizeof( dayHigh ) );
                memcpy( dayLow, c.dayLow, sizeof( dayLow ) );
                memcpy( shiftHigh, c.shiftHigh, sizeof( shiftHigh ) );
                memcpy( shiftLow, c.shiftLow, sizeof( shiftLow ) );
            }
            Consecutive& operator=(const Consecutive &c)
            {
                memcpy( dayHigh, c.dayHigh, sizeof( dayHigh ) );
                memcpy( dayLow, c.dayLow, sizeof( dayLow ) );
                memcpy( shiftHigh, c.shiftHigh, sizeof( shiftHigh ) );
                memcpy( shiftLow, c.shiftLow, sizeof( shiftLow ) );
                return *this;
            }

            // if a shift lasts whole week, return true, else false
            bool isSingleConsecutiveShift() const
            {   // shiftLow may be HIS, so use less or equal
                return (shiftLow[Weekday::Sun] <= Weekday::Mon);
            }
            // if a day or day-off lasts whole week, return true, else false
            bool isSingleConsecutiveDay() const
            {   // dayLow may be HIS, so use less or equal
                return (dayLow[Weekday::Sun] <= Weekday::Mon);
            }

            int dayLow[Weekday::SIZE];
            int dayHigh[Weekday::SIZE];
            int shiftLow[Weekday::SIZE];
            int shiftHigh[Weekday::SIZE];

        private:    // forbidden operators
        };

        // single move in neighborhood search
        class Move
        {
        public:
            Move() : delta( MAX_OBJ_VALUE ) {}
            Move( ObjValue d, NurseID n, int w )
                : delta( d ), nurse( n ), weekday( w )
            {
            }
            Move( ObjValue d, NurseID n, int w, ShiftID sh, SkillID sk )
                : delta( d ), nurse( n ), weekday( w ), shift( sh ), skill( sk )
            {
            }

            ObjValue delta;
            NurseID nurse;
            int weekday;
            ShiftID shift;
            SkillID skill;
        };


        // depth first search to fill assign
        bool fillAssign( int weekday, ShiftID shift, SkillID skill, NurseID nurse, int nurseNum );

        // find day number to be punished for a single block
        // work for shift, day and day-off
        static ObjValue penaltyDayNum( int len, int end, int min, int max )
        {
            return (end < Weekday::Sun) ?
                distanceToRange( len, min, max ) :
                exceedCount( len, max );
        }

        // evaluate cost of adding a shift to nurse without shift in weekday
        ObjValue tryAddShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill ) const;
        // evaluate cost of assigning another shift or skill to nurse already assigned in weekday
        ObjValue tryChangeShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill ) const;
        // evaluate cost of removing the shift from nurse already assigned in weekday
        ObjValue tryRemoveShift( int weekday, NurseID nurse ) const;
        // apply assigning a shift to nurse without shift in weekday
        void addShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill );
        // apply assigning another shift or skill to nurse already assigned in weekday
        void changeShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill );
        // apply removing a shift to nurse in weekday
        void removeShift( int weekday, NurseID nurse );

        void updateConsecutive( int weekday, NurseID nurse, ShiftID shift );
        // the assignment is on the right side of a consecutive block
        void assignHigh( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight );
        // the assignment is on the left side of a consecutive block
        void assignLow( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectLeft );
        // the assignment is in the middle of a consecutive block
        void assignMiddle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE] );
        // the assignment is on a consecutive block with single slot
        void assignSingle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight, bool affectLeft );

        // return true if the solution will be improved (delta < 0)
        bool findBestAddShift( Move &bestMove ) const;
        bool findBestChangeShift( Move &bestMove ) const;
        bool findBestRemoveShift( Move &bestMove ) const;
        bool findBestAddShiftOnConsecutiveBorder( Move &bestMove ) const;
        bool findBestChangeShiftOnConsecutiveBorder( Move &bestMove ) const;
        bool findBestRemoveShiftOnConsecutiveBorder( Move &bestMove ) const;

        void evaluateInsufficientStaff();
        void evaluateConsecutiveShift();
        void evaluateConsecutiveDay();
        void evaluateConsecutiveDayOff();
        void evaluatePreference();
        void evaluateCompleteWeekend();
        void evaluateTotalAssign();
        void evaluateTotalWorkingWeekend();

        const TabuSolver &solver;

        // total assignments for each nurse
        std::vector<int> totalAssignNums;
        // missing nurse numbers for each single assignment
        NurseNumsOnSingleAssign missingNurseNums;
        // consecutive[nurse] is the consecutive assignments record for nurse
        std::vector<Consecutive> consecutives;

        Assign assign;

        ObjValue objValue;
        ObjValue objInsufficientStaff;
        ObjValue objConsecutiveShift;
        ObjValue objConsecutiveDay;
        ObjValue objConsecutiveDayOff;
        ObjValue objPreference;
        ObjValue objCompleteWeekend;
        ObjValue objTotalAssign;
        ObjValue objTotalWorkingWeekend;

    private:    // forbidden operators
        Solution& operator=(const Solution &) { return *this; }
    };


    Solution sln;

    // nurseNumOfSkill[skill] is the number of nurses with that skill
    std::vector<SkillID> nurseNumOfSkill;
    NurseWithSkill nurseWithSkill;

private:    // forbidden operators
    TabuSolver& operator=(const TabuSolver &) { return *this; }
};


#endif