/**
*   usage : 1.  provide interface to be manipulated by
*               solver for Nurse Rostering Problem.
*
*   note :  v1.  use repair mode of penalty to fix infeasible solution in repair().
*           v2.  merge add and remove to a single neighborhood, abreast of swap and change.
*           3.  const solver will not be able to call updateOptima,
*               but none-const solver will stub the construction in solver.genHistory().
*           4.  create a fastRepair() which is separate repair and local search
*               to improve efficiency since repair process is more similar to generating initial assignment?
*           5.  perturb() should start from optima?
*
*/

#ifndef SOLUTION_H
#define SOLUTION_H


#include <algorithm>
#include <vector>
#include <bitset>
#include <ctime>

#include "DebugFlag.h"
#include "NurseRostering.h"
#include "utility.h"


class NurseRostering::Output
{
public:
    Output() :objValue( -1 ) {}
    Output( ObjValue objVal, const AssignTable &assignment )
        :objValue( objVal ), assign( assignment )
    {
    }

    const Assign& getAssign( NurseID nurse, int weekday ) const
    {
        return assign[nurse][weekday];
    }
    const AssignTable& getAssignTable() const { return assign; }
    ObjValue getObjValue() const { return objValue; }

protected:
    AssignTable assign;
    ObjValue objValue;
};


class NurseRostering::Solution : public NurseRostering::Output
{
public:
    // single move in neighborhood search
    class Move
    {
    public:
        // fundamental move modes in local search, SIZE is the number of move types
        // AR stands for "Add and Remove", "Rand" means select one to search randomly,
        // "Both" means search both, "Loop" means switch to another when no improvement
        enum Mode
        {
            Add, Change, Swap, Remove, BASIC_MOVE_SIZE,
            ARLoop = BASIC_MOVE_SIZE, ARRand, ARBoth, SIZE
        };

        Move() : delta( DefaultPenalty::FORBIDDEN_MOVE ) {}
        Move( ObjValue d, int w, NurseID n, Mode m = Mode::Remove )
            : delta( d ), weekday( w ), nurse( n ), mode( m )
        {
        }
        Move( ObjValue d, int w, NurseID n, const Assign &a, Mode m )
            : delta( d ), weekday( w ), nurse( n ), assign( a ), mode( m )
        {
        }
        Move( ObjValue d, int w, NurseID n, ShiftID sh, SkillID sk, Mode m )
            : delta( d ), weekday( w ), nurse( n ), assign( sh, sk ), mode( m )
        {
        }
        Move( ObjValue d, int w, NurseID n, NurseID  n2, Mode m = Mode::Swap )
            : delta( d ), weekday( w ), nurse( n ), nurse2( n2 ), mode( m )
        {
        }

        ObjValue delta;
        Mode mode;
        int weekday;
        NurseID nurse;
        NurseID nurse2;
        Assign assign;
    };

    typedef ObjValue( Solution::*TryMove )(const Move &move) const;
    typedef bool (Solution::*FindBestMove)(Move &move) const;
    typedef void (Solution::*ApplyMove)(const Move &move);
    typedef void (Solution::*UpdateTabu)(const Move &move);

    typedef std::vector<TryMove> TryMoveTable;
    typedef std::vector<FindBestMove> FindBestMoveTable;
    typedef std::vector<ApplyMove> ApplyMoveTable;
    typedef std::vector<UpdateTabu> UpdateTabuTable;


    enum ModeSeq
    {
        ARLCS, ARRCS, ARBCS, ACSR, ASCR, SIZE
    };

    static const std::vector<std::string> modeSeqNames;
    static const std::vector<std::vector<int> > modeSeqPatterns;

    static const TryMoveTable tryMove;
    static const FindBestMoveTable findBestMove;
    static const FindBestMoveTable findBestMoveOnBlockBorder;
    static const ApplyMoveTable applyMove;
    static const UpdateTabuTable updateTabuTable;


    Solution( const TabuSolver &solver );
    Solution( const TabuSolver &solver, const AssignTable &assign );

    // set assign to at and rebuild assist data, at must be build from same problem
    void rebuild( const AssignTable &at );
    // evaluate objective by assist data structure
    // must be called after Penalty change or direct access to AssignTable
    void evaluateObjValue();
    // get history for next week, only used for custom file
    History genHistory() const;

    bool genInitAssign( int greedyRetryCount );
    bool genInitAssign_Greedy();
    bool genInitAssign_BranchAndCut();
    bool repair( const Timer &timer );  // make infeasible solution feasible


    // randomly select neighborhood to search until timeout.
    // for each neighborhood i, the possibility to select is P[i].
    // increase the possibility to select when no improvement.
    // in detail, the P[i] contains two part, local and global.
    // the local part will increase if the neighborhood i makes
    // improvement, and decrease vice versa. the global part will
    // increase if recent search (not only on neighborhood i)
    // can not make improvement, otherwise, it will decrease.
    // in case the iteration takes too much time, it can be changed
    // from best improvement to first improvement.
    // if no neighborhood has been selected, prepare a loop queue.
    // select the one by one in the queue until a valid move is found.
    // move the head to the tail of the queue if it makes no improvement.
    void tabuSearch( const Timer &timer, const FindBestMoveTable &findBestMoveTable );
    // try add shift until there is no improvement , then try change shift,
    // then try remove shift, then try add shift again. if all of them
    // can't improve or time is out, return.
    void localSearch( const Timer &timer, const FindBestMoveTable &findBestMoveTable );
    // change solution structure in certain complexity
    void perturb( double strength );
    // randomly select add, change or remove shift until timeout
    void randomWalk( const Timer &timer, IterCount stepNum );

    const AssignTable& getAssignTable() const { return assign; }
    // shift must not be none shift
    bool isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const;
    bool isValidPrior( NurseID nurse, ShiftID shift, int weekday ) const;

    // check if the result of incremental update, evaluate and checkObjValue is the same
    bool checkIncrementalUpdate();

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

        AvailableNurses( const Solution &s );
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
    // fine-grained tabu list for add or remove on each shift
    // (iterCount <= ShiftTabu[nurse][weekday][shift][skill]) means forbid to be added
    typedef std::vector< std::vector< std::vector< std::vector<IterCount> > > > ShiftTabu;
    // coarse-grained tabu list for add or remove on each day
    // (iterCount <= DayTabu[nurse][weekday]) means forbid to be removed
    typedef std::vector< std::vector<IterCount> > DayTabu;


    // find day number to be punished for a single block
    // work for shift, day and day-off
    static ObjValue penaltyDayNum( int len, int end, int min, int max )
    {
        return (end < Weekday::Sun) ?
            distanceToRange( len, min, max ) :
            exceedCount( len, max );
    }

    // depth first search to fill assign
    bool fillAssign( int weekday, ShiftID shift, SkillID skill, NurseID nurse, int nurseNum );

    // allocate space for data structure and set default value
    // must be called before generating initial solution
    void resetAssign();
    void resetAssistData();
    // must be called after direct access to AssignTable
    void rebuildAssistData();


    // return true if the solution will be improved (delta < 0)
    // BlockBorder means the start or end day of a consecutive block
    bool findBestAdd( Move &bestMove ) const;
    bool findBestChange( Move &bestMove ) const;
    bool findBestRemove( Move &bestMove ) const;
    bool findBestSwap( Move &bestMove ) const;
    bool findBestARLoop( Move &bestMove ) const;
    bool findBestARRand( Move &bestMove ) const;
    bool findBestARBoth( Move &bestMove ) const;
    bool findBestAddOnBlockBorder( Move &bestMove ) const;
    bool findBestChangeOnBlockBorder( Move &bestMove ) const;
    bool findBestRemoveOnBlockBorder( Move &bestMove ) const;
    bool findBestSwapOnBlockBorder( Move &bestMove ) const;
    bool findBestARLoopOnBlockBorder( Move &bestMove ) const;
    bool findBestARRandOnBlockBorder( Move &bestMove ) const;
    bool findBestARBothOnBlockBorder( Move &bestMove ) const;

    // evaluate cost of adding a Assign to nurse without Assign in weekday
    ObjValue tryAddAssign( int weekday, NurseID nurse, const Assign &a ) const;
    ObjValue tryAddAssign( const Move &move ) const;
    // evaluate cost of assigning another Assign or skill to nurse already assigned in weekday
    ObjValue tryChangeAssign( int weekday, NurseID nurse, const Assign &a ) const;
    ObjValue tryChangeAssign( const Move &move ) const;
    // evaluate cost of removing the Assign from nurse already assigned in weekday
    ObjValue tryRemoveAssign( int weekday, NurseID nurse ) const;
    ObjValue tryRemoveAssign( const Move &move ) const;
    // evaluate cost of swapping Assign of two nurses
    ObjValue trySwapNurse( int weekday, NurseID nurse, NurseID nurse2 ) const;
    ObjValue trySwapNurse( const Move &move ) const;
    // apply assigning a Assign to nurse without Assign in weekday
    void addAssign( int weekday, NurseID nurse, const Assign &a );
    void addAssign( const Move &move );
    // apply assigning another Assign or skill to nurse already assigned in weekday
    void changeAssign( int weekday, NurseID nurse, const Assign &a );
    void changeAssign( const Move &move );
    // apply removing a Assign to nurse in weekday
    void removeAssign( int weekday, NurseID nurse );
    void removeAssign( const Move &move );
    // apply swapping Assign of two nurses
    void swapNurse( int weekday, NurseID nurse1, NurseID nurse2 );
    void swapNurse( const Move &move );

    void updateConsecutive( int weekday, NurseID nurse, ShiftID shift );
    // the assignment is on the right side of a consecutive block
    void assignHigh( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight );
    // the assignment is on the left side of a consecutive block
    void assignLow( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectLeft );
    // the assignment is in the middle of a consecutive block
    void assignMiddle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE] );
    // the assignment is on a consecutive block with single slot
    void assignSingle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight, bool affectLeft );

    bool noAddTabu( const Move &move ) const
    {
        return (iterCount > shiftTabu[move.nurse][move.weekday][move.assign.shift][move.assign.skill]);
    }
    bool noChangeTabu( const Move &move ) const
    {
        return (iterCount > shiftTabu[move.nurse][move.weekday][move.assign.shift][move.assign.skill]);
    }
    bool noRemoveTabu( const Move &move ) const
    {
        return (iterCount > dayTabu[move.nurse][move.weekday]);
    }
    bool noSwapTabu( const Move &move ) const
    {
        const Assign &a( assign[move.nurse][move.weekday] );
        const Assign &a2( assign[move.nurse2][move.weekday] );

        if (a.isWorking()) {
            if (a2.isWorking()) {
                return ((iterCount > shiftTabu[move.nurse][move.weekday][a2.shift][a2.skill])
                    || (iterCount > shiftTabu[move.nurse2][move.weekday][a.shift][a.skill]));
            } else {
                return ((iterCount > dayTabu[move.nurse][move.weekday])
                    || (iterCount > shiftTabu[move.nurse2][move.weekday][a.shift][a.skill]));
            }
        } else {
            if (a2.isWorking()) {
                return ((iterCount > shiftTabu[move.nurse][move.weekday][a2.shift][a2.skill])
                    || (iterCount > dayTabu[move.nurse2][move.weekday]));
            } else {    // no change
                return true;
            }
        }
    }

    void updateDayTabu( NurseID nurse, int weekday );
    void updateShiftTabu( NurseID nurse, int weekday, const Assign &a );
    void updateAddTabu( const Move &move )
    {
        updateDayTabu( move.nurse, move.weekday );
    }
    void updateChangeTabu( const Move &move )
    {
        updateShiftTabu( move.nurse, move.weekday, assign[move.nurse][move.weekday] );
    }
    void updateRemoveTabu( const Move &move )
    {
        updateShiftTabu( move.nurse, move.weekday, assign[move.nurse][move.weekday] );
    }
    void updateSwapTabu( const Move &move )
    {
        const Assign &a( assign[move.nurse][move.weekday] );
        const Assign &a2( assign[move.nurse][move.weekday] );

        if (a.isWorking()) {
            if (a2.isWorking()) {
                updateShiftTabu( move.nurse, move.weekday, a );
                updateShiftTabu( move.nurse2, move.weekday, a2 );
            } else {
                updateShiftTabu( move.nurse, move.weekday, a );
                updateDayTabu( move.nurse2, move.weekday );
            }
        } else {
            if (a2.isWorking()) {
                updateDayTabu( move.nurse, move.weekday );
                updateShiftTabu( move.nurse2, move.weekday, a2 );
            }
        }
    }

    void evaluateInsufficientStaff();
    void evaluateConsecutiveShift();
    void evaluateConsecutiveDay();
    void evaluateConsecutiveDayOff();
    void evaluatePreference();
    void evaluateCompleteWeekend();
    void evaluateTotalAssign();
    void evaluateTotalWorkingWeekend();

    const TabuSolver &solver;

    mutable Penalty penalty;

    // for switching between add and remove
    // 1 for no improvement which is opposite in localSearch
    mutable bool findBestARLoopFlag;
    mutable bool findBestARLoopOnBlockBorderFlag;

    ShiftTabu shiftTabu;
    DayTabu dayTabu;
    IterCount iterCount;

    // total assignments for each nurse
    std::vector<int> totalAssignNums;
    // missing nurse numbers for each single assignment
    NurseNumsOnSingleAssign missingNurseNums;
    // consecutive[nurse] is the consecutive assignments record for nurse
    std::vector<Consecutive> consecutives;

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



#endif
