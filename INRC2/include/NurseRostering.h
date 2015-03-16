/**
*   usage : 1. provide solver interface for Nurse Rostering Problem.
*
*   note :  1. [optimizable] Solver has virtual function.
*           2. set algorithm arguments in run().
*           3. use a priority queue to manage available nurse when assigning?
*           4. record 8 day which the first day is last day of last week to unify
*              the succession judgment.
*           5. [optimizable] put evaluateObjValue() in initAssign() or call it in init() ?
*           6. [optimizable] evaluateConsecutiveDay() and evaluateConsecutiveDayOff() can
*               be put together and not consider if there is an assignment. but there is
*               a difficult problem with consecutive state on the beginning of the week.
*/

#ifndef NURSE_ROSTERING_H
#define NURSE_ROSTERING_H


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>
#include <map>
#include <set>
#include <algorithm>
#include <cstdlib>

#include "utility.h"


class NurseRostering
{
public:
    static const int MAX_RUNNING_TIME = 1073741824;  // in millisecond
    enum Weekday { Mon = 0, Tue, Wed, Thu, Fri, Sat, Sun, NUM };
    enum Penalty
    {
        AMP = 1000,
        InsufficientStaff = (AMP * 30),
        ConsecutiveShift = (AMP * 15),
        ConsecutiveDay = (AMP * 30),
        ConsecutiveDayOff = (AMP * 30),
        Preference = (AMP * 10),
        CompleteWeekend = (AMP * 30),
        TotalAssign = (AMP * 20),
        TotalWorkingWeekend = (AMP * 30)
    };


    typedef int ObjValue;   // unit of objective function
    typedef int NurseID;    // non-negative number for a certain nurse
    typedef int ContractID; // non-negative number for a certain contract
    typedef int ShiftID;    // NONE, ANY or non-negative number for a certain kind of shift
    typedef int SkillID;    // non-negative number for a certain kind of skill

    // NurseNumsOnSingleAssign[day][shift][skill] is a number of nurse
    typedef std::vector< std::vector< std::vector<int> > > NurseNumsOnSingleAssign;

    class Scenario
    {
    public:
        // if there are weekNum weeks in the planning horizon, maxWeekCount = (weekNum - 1) 
        int maxWeekCount;   // count from 0
        int totalWeekNum;   // count from 1
        int shiftTypeNum;
        int skillTypeNum;
        int nurseNum;

        class Shift
        {
        public:
            static const ShiftID ID_ANY;
            static const std::string NAME_ANY;
            static const ShiftID ID_NONE;
            static const std::string NAME_NONE;

            int minConsecutiveShiftNum;
            int maxConsecutiveShiftNum;
            // (illegalNextShift[nextShift] == true) means nextShift 
            // is available to be succession of this shift
            std::vector<bool> legalNextShifts;
        };
        std::vector<Shift> shifts;

        class Contract
        {
        public:
            int minShiftNum;    // total assignments in the planning horizon
            int maxShiftNum;    // total assignments in the planning horizon
            int minConsecutiveWorkingDayNum;
            int maxConsecutiveWorkingDayNum;
            int minConsecutiveDayoffNum;
            int maxConsecutiveDayoffNum;
            int maxWorkingWeekendNum;   // total assignments in the planning horizon
            bool completeWeekend;
        };
        std::vector<Contract> contracts;

        class Nurse
        {
        public:
            static const NurseID ID_NONE;

            ContractID contract;
            std::vector<SkillID> skills;
        };
        std::vector<Nurse> nurses;
    };

    class WeekData
    {
    public:
        // (shiftOffs[day][shift][nurse] == true) means shiftOff required
        std::vector< std::vector< std::vector<bool> > > shiftOffs;
        // optNurseNums[day][shift][skill] is a number of nurse
        NurseNumsOnSingleAssign optNurseNums;
        // optNurseNums[day][shift][skill] is a number of nurse
        NurseNumsOnSingleAssign minNurseNums;
    };

    // history.XXX[nurse] is the XXX of a certain nurse
    class History
    {
    public:
        std::vector<int> shiftNum;
        std::vector<int> workingWeekendNum;
        std::vector<ShiftID> lastShift;
        std::vector<int> consecutiveShiftNum;
        std::vector<int> consecutiveWorkingDayNum;
        std::vector<int> consecutiveDayoffNum;
    };

    class Names
    {
    public:
        std::string scenarioName;

        std::vector<std::string> skillNames;    // skillMap[skillNames[skillID]] == skillID
        std::map<std::string, SkillID> skillMap;

        std::vector<std::string> shiftNames;    // shiftMap[shiftNames[shiftID]] == shiftID
        std::map<std::string, ShiftID> shiftMap;

        std::vector<std::string> contractNames; // contractMap[contractNames[contractID]] == contractID
        std::map<std::string, ContractID> contractMap;

        std::vector<std::string> nurseNames;    // nurseMap[nurseNames[nurseID]] == nurseID
        std::map<std::string, NurseID> nurseMap;
    };

    class SingleAssign
    {
    public:
        // the default constructor means there is no assignment
        SingleAssign( ShiftID sh = Scenario::Shift::ID_NONE, SkillID sk = 0 ) :shift( sh ), skill( sk ) {}

        ShiftID shift;
        SkillID skill;
    };
    // Assign[nurse][day] is a SingleAssign
    class Assign : public std::vector < std::vector< SingleAssign > >
    {
    public:
        Assign() {}
        Assign( int nurseNum, int weekdayNum = Weekday::NUM, const SingleAssign &singleAssign = SingleAssign() )
            : std::vector< std::vector< SingleAssign > >( nurseNum, std::vector< SingleAssign >( weekdayNum, singleAssign ) ) {}

        static bool isWorking( ShiftID shift )
        {
            return (shift != NurseRostering::Scenario::Shift::ID_NONE);
        }

        bool isWorking( NurseID nurse, int weekday ) const
        {
            return isWorking( at( nurse ).at( weekday ).shift );
        }
    private:

    };

    class Solver
    {
    public:
        static const int ILLEGAL_SOLUTION = -1;
        static const int CHECK_TIME_INTERVAL_MASK_IN_ITER = ((1 << 10) - 1);
        static const int SAVE_SOLUTION_TIME_IN_MILLISECOND = 50;

        class Output
        {
        public:
            Output() :objVal( -1 ) {}
            Output( int objValue, const Assign &assignment )
                :objVal( objValue ), assign( assignment ), findTime( clock() )
            {
            }

            Assign assign;
            ObjValue objVal;
            clock_t findTime;
        };

        // set algorithm name, set parameters, generate initial solution
        virtual void init() = 0;
        // search for optima
        virtual void solve() = 0;
        // return const reference of the optima
        const Output& getOptima() const { return optima; }
        // print simple information of the solution to console
        void print() const;
        // record solution to specified file and create custom file if required
        void record( const std::string logFileName, const std::string &instanceName ) const;  // contain check()
        // return true if the optima solution is feasible and objValue is the same
        bool check() const;

        // calculate objective of optima with original input instead of auxiliary data structure
        // return objective value if solution is legal, else ILLEGAL_SOLUTION
        bool checkFeasibility( const Assign &assgin ) const;
        bool checkFeasibility() const;  // check optima assign
        int checkObjValue( const Assign &assign ) const;
        int checkObjValue() const;  // check optima assign

        Solver( const NurseRostering &input, const std::string &name, clock_t startTime );
        virtual ~Solver();


        const NurseRostering &problem;

    protected:
        // solver will check time every certain number of iterations
        // this determines if it is the right iteration to check time
        static bool isIterForTimeCheck( int iterCount )
        {
            return (!(iterCount & CHECK_TIME_INTERVAL_MASK_IN_ITER));
        }
        // create header of the table ( require ios::app flag or "a" mode )
        static void initResultSheet( std::ofstream &csvFile );

        NurseNumsOnSingleAssign countNurseNums( const Assign &assign ) const;
        void checkConsecutiveViolation( int &objValue,
            const Assign &assign, NurseID nurse, int weekday, ShiftID lastShiftID,
            int &consecutiveShift, int &consecutiveDay, int &consecutiveDayOff,
            bool &shiftBegin, bool &dayBegin, bool &dayoffBegin ) const;


        Output optima;

        std::string algorithmName;
        clock_t startTime;
        clock_t endTime;
        int iterCount;
        int generationCount;
    };

    class TabuSolver : public Solver
    {
    public:
        static const std::string Name;

        virtual void init();
        virtual void solve();

        // initialize data about nurse-skill relation
        void initAssistData();  // initialize nurseWithSkill, nurseNumOfSkill
        TabuSolver( const NurseRostering &input, clock_t startTime = clock() );
        virtual ~TabuSolver();

    private:
        // NurseWithSkill[skill][skillNum-1] is a set of nurses 
        // who have that skill and have skillNum skills in total
        typedef std::vector< std::vector<std::vector<NurseID> > > NurseWithSkill;

        class Solution
        {
        public:
            bool genInitAssign();   // assign must be default value( call resetAssign() )
            void resetAssign();     // reset all shift to Shift::ID_NONE
            void evaluateObjValue();
            void repair();
            void searchNeighborhood();

            const Assign& getAssign() const { return assign; }
            // shift must not be none shift
            bool isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const;

            Solution( TabuSolver &solver );
            operator Output() const
            {
                return Output( objValue, assign );
            }

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
            };

            // consecutive information for a nurse
            //==========================================
            // this is an demo for switching consecutive working day and day off.
            // consecutive assignments use the same method.
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
                Consecutive()
                {
                    std::fill( dayLow, dayLow + Weekday::NUM, Weekday::Mon );
                    std::fill( dayHigh, dayHigh + Weekday::NUM, Weekday::Sun );
                    std::fill( shiftLow, shiftLow + Weekday::NUM, Weekday::Mon );
                    std::fill( shiftHigh, shiftHigh + Weekday::NUM, Weekday::Sun );
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

                int dayLow[Weekday::NUM];
                int dayHigh[Weekday::NUM];
                int shiftLow[Weekday::NUM];
                int shiftHigh[Weekday::NUM];
            };


            ObjValue testAssignShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill );
            ObjValue testRemoveShift( int weekday, NurseID nurse );
            void assignShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill );
            void removeShift( int weekday, NurseID nurse );

            void updateConsecutive( int weekday, NurseID nurse, ShiftID shift );
            void assignHigh( int weekday, int nextDay, int prevDay, int high[Weekday::NUM], int low[Weekday::NUM], bool affectRight );
            void assignLow( int weekday, int nextDay, int prevDay, int high[Weekday::NUM], int low[Weekday::NUM], bool affectLeft );
            void assignMiddle( int weekday, int nextDay, int prevDay, int high[Weekday::NUM], int low[Weekday::NUM] );

            void evaluateInsufficientStaff();
            void evaluateConsecutiveShift();
            void evaluateConsecutiveDay();
            void evaluateConsecutiveDayOff();
            void evaluatePreference();
            void evaluateCompleteWeekend();
            void evaluateTotalAssign();
            void evaluateTotalWorkingWeekend();

            TabuSolver &solver;

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
        };


        Solution sln;

        // nurseNumOfSkill[skill] is the number of nurses with that skill
        std::vector<SkillID> nurseNumOfSkill;
        NurseWithSkill nurseWithSkill;
    };


    static const ObjValue MAX_OBJ_VALUE;


    // must set all data members by direct accessing!
    NurseRostering();


    // data to identify a nurse rostering problem
    int randSeed;
    int timeout;        // time in millisecond
    int pastWeekCount;  // count from 0 (the number in history file)
    int currentWeek;    // count from 1
    WeekData weekData;
    Scenario scenario;
    History history;
    Names names;

private:

};


#endif
