/**
*   usage : 1.  provide interface to be manipulated by
*               solver for Nurse Rostering Problem.
*
*   note :  v1.  use repair mode of penalty to fix infeasible solution in repair().
*           v2.  merge add and remove to a single neighborhood, abreast of swap and change.
*           x3.  const solver will not be able to call updateOptima,
*               but none-const solver will stub the construction in solver.genHistory().
*           4.  create a fastRepair() which is separate repair and local search
*               to improve efficiency since repair process is more similar to generating initial assignment?
*           v5.  perturb() should start from optima?
*           v6.  move mode setting to findBest rather than try.
*           7.  just add max tabu tenure to iterCount rather than reset tabu table when rebuild (except the first time)
*           8.  rebuild() may not function well if diff is too big or small.
*           v9.  there will be problem on total assignment violation calculation if
*               total assignment in initial history is not 0.
*
*/

#ifndef SOLUTION_H
#define SOLUTION_H


#include <algorithm>
#include <vector>
#include <queue>
#include <ctime>
#include <cmath>
#include <cstring>

#include "DebugFlag.h"
#include "NurseRostering.h"
#include "utility.h"


class NurseRostering::Output
{
public:
    // assume initial solution will never be the global optima
    Output() : objValue( DefaultPenalty::FORBIDDEN_MOVE ), secondaryObjValue( DefaultPenalty::FORBIDDEN_MOVE ) {}
    Output( ObjValue objVal, const AssignTable &assignment, ObjValue secondaryObjVal = DefaultPenalty::FORBIDDEN_MOVE )
        : objValue( objVal ), secondaryObjValue( secondaryObjVal ), assign( assignment ), findTime( clock() )
    {
    }

    const Assign& getAssign( NurseID nurse, int weekday ) const
    {
        return assign[nurse][weekday];
    }
    const AssignTable& getAssignTable() const { return assign; }
    ObjValue getObjValue() const { return objValue; }
    double getSecondaryObjValue() const { return secondaryObjValue; }
    clock_t getFindTime() const { return findTime; }

protected:
    AssignTable assign;
    ObjValue objValue;
    double secondaryObjValue;
    clock_t findTime;
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
            // atomic moves are not composed by other moves
            Add, Remove, Change, ATOMIC_MOVE_SIZE,
            // basic moves are used in randomWalk()
            Swap = ATOMIC_MOVE_SIZE, Exchange, BlockSwap, BASIC_MOVE_SIZE,
            // compound moves which can not be used by a single tryXXX()
            BlockShift = BASIC_MOVE_SIZE, ARLoop, ARRand, ARBoth, SIZE
        };

        Move() : delta( DefaultPenalty::FORBIDDEN_MOVE ) {}
        Move( ObjValue d, int w, int w2, NurseID n, NurseID n2 )
            : delta( d ), weekday( w ), weekday2( w2 ), nurse( n ), nurse2( n2 )
        {
        }

        friend bool operator<(const Move &l, const Move &r)
        {
            return (l.delta == r.delta) ? (rand() % 2 == 0) : (l.delta > r.delta);
        }

        ObjValue delta;
        Mode mode;
        mutable int weekday;    // tryBlockSwap() will modify it
        // weekday2 should always be greater than weekday in block swap
        mutable int weekday2;   // tryBlockSwap() will modify it
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

    typedef void (Solution::*Search)(const Timer &timer, const FindBestMoveTable &findBestMoveTable);


    enum ModeSeq
    {
        ARlCS, ARrCS, ARbCS, ACSR,
        ARlSCB, ARrSCB, ARbSCB, ASCBR,
        ARlCSE, ARrCSE, ARbCSE, ACSER,
        ARlCSEB, ARrCSEB, ARbCSEB, ACSEBR,
        ARlCB, ARrCB, ARbCB, ACBR,
        ARlCEB, ARrCEB, ARbCEB, ACEBR, SIZE
    };

    static const std::vector<std::string> modeSeqNames;
    static const std::vector<std::vector<int> > modeSeqPatterns;

    // entrance in this table should have the same sequence
    // as move mode enum defined in Move::Mode
    static const TryMoveTable tryMove;
    static const FindBestMoveTable findBestMove;
    static const FindBestMoveTable findBestMoveOnBlockBorder;
    static const ApplyMoveTable applyMove;
    static const UpdateTabuTable updateTabuTable;

    static const double NO_DIFF;    // for building same assign in rebuild()

    const TabuSolver &solver;
    const NurseRostering &problem;


    Solution( const TabuSolver &solver );
    Solution( const TabuSolver &solver, const Output &output );

    // get local optima in the search trajectory
    const Output& getOptima() const { return optima; }
    // return true if update succeed
    bool updateOptima()
    {
#ifdef INRC2_SECONDARY_OBJ_VALUE
        if (objValue <= optima.getObjValue()) {
            secondaryObjValue = 0;
            for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
                secondaryObjValue += static_cast<double>(totalAssignNums[nurse]) / problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxShiftNum;
            }
        }
#endif
        if (objValue < optima.getObjValue()) {
            findTime = clock();
            optima = *this;
            return true;
        } else if (objValue == optima.getObjValue()) {
#ifdef INRC2_SECONDARY_OBJ_VALUE
            bool isSelected = (secondaryObjValue < optima.getSecondaryObjValue());
#else
            bool isSelected = ((rand() % 2) == 0);
#endif
            if (isSelected) {
                findTime = clock();
                optima = *this;
                return true;
            }
        }

        return false;
    }
    // set assign to at and rebuild assist data, at must be build from same problem
    void rebuild( const Output &output, double diff );  // objValue will be recalculated
    void rebuild( const Output &output );   // objValue is copied from output
    void rebuild(); // must be called after direct access to AssignTable (objValue will be recalculated)
    // evaluate objective by assist data structure
    // must be called after Penalty change or direct access to AssignTable
    void evaluateObjValue();
    // get history for next week, only used for custom file
    History genHistory() const;
    // get current iteration count (may not be the actual iter count)
    IterCount getIterCount() const { return iterCount; }

    bool genInitAssign( int greedyRetryCount );
    bool genInitAssign_Greedy();
    bool genInitAssign_BranchAndCut();
    bool repair( const Timer &timer );  // make infeasible solution feasible


    // try to start with a best block swap or a best block swap for single nurse
    // until no improvement on the optima
    void swapChainSearch_DoubleHead( const Timer &timer, IterCount maxNoImproveChainLength );
    // iteratively call genSwapChain() with different start link
    // until given count of no improvement.
    void swapChainSearch( const Timer &timer, IterCount maxNoImproveChainLength );
    // start with a given block swap (will improve at least one nurse), 
    // the (more) improved nurse is the head of the link which
    // the other one is the tail and the head of next link.
    // each tail has two branches. the first one is make add, remove, change
    // or block shift which will stop the chain if improved. the second one is to 
    // find a block swap to improve this nurse which will continue the chain.
    bool genSwapChain( const Timer &timer, const Move &head, IterCount noImproveLen );
    // select single neighborhood to search in each iteration randomly
    // the random select process is a discrete distribution
    // the possibility to be selected will increase if the neighborhood
    // improve the solution, else decrease it. the sum of possibilities is 1.0
    void tabuSearch_Rand( const Timer &timer, const FindBestMoveTable &findBestMoveTable );
    // loop to select neighborhood to search until timeout or there is no
    // improvement on (NeighborhoodNum + 2) neighborhood consecutively.
    // switch neighborhood when maxNoImproveForSingleNeighborhood has
    // been reach, then restart from optima in current trajectory.
    void tabuSearch_Loop( const Timer &timer, const FindBestMoveTable &findBestMoveTable );
    // randomly select neighborhood to search until timeout or
    // no improve move count reaches maxNoImproveForAllNeighborhood.
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
    void tabuSearch_Possibility( const Timer &timer, const FindBestMoveTable &findBestMoveTable );
    // try add shift until there is no improvement , then try change shift,
    // then try remove shift, then try add shift again. if all of them
    // can't improve or time is out, return.
    void localSearch( const Timer &timer, const FindBestMoveTable &findBestMoveTable );
    // randomly select add, change or remove shift until timeout
    void randomWalk( const Timer &timer, IterCount stepNum );

    // change solution structure in certain complexity
    void perturb( double strength );

    const AssignTable& getAssignTable() const { return assign; }
    // shift must not be none shift
    bool isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const
    {
        return problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[shift];
    }
    bool isValidPrior( NurseID nurse, ShiftID shift, int weekday ) const
    {
        return problem.scenario.shifts[shift].legalNextShifts[assign[nurse][weekday + 1].shift];
    }

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
    // must be called before building up a solution
    void resetAssign();
    void resetAssistData();


    // return true if the solution will be improved (delta < 0)
    // BlockBorder means the start or end day of a consecutive block
    bool findBestAdd( Move &bestMove ) const;
    bool findBestChange( Move &bestMove ) const;
    bool findBestRemove( Move &bestMove ) const;
    bool findBestSwap( Move &bestMove ) const;
    bool findBestBlockSwap( Move &bestMove ) const;         // try all nurses
    bool findBestBlockSwap_fast( Move &bestMove ) const;    // try all nurses
    bool findBestBlockSwap_part( Move &bestMove ) const;    // try some nurses following index
    bool findBestBlockSwap_rand( Move &bestMove ) const;    // try randomly picked nurses 
    bool findBestExchange( Move &bestMove ) const;
    bool findBestBlockShift( Move &bestMove ) const;
    bool findBestARLoop( Move &bestMove ) const;
    bool findBestARRand( Move &bestMove ) const;
    bool findBestARBoth( Move &bestMove ) const;
    bool findBestAddOnBlockBorder( Move &bestMove ) const;
    bool findBestChangeOnBlockBorder( Move &bestMove ) const;
    bool findBestRemoveOnBlockBorder( Move &bestMove ) const;
    bool findBestSwapOnBlockBorder( Move &bestMove ) const;
    bool findBestExchangeOnBlockBorder( Move &bestMove ) const;
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
    // evaluate cost of swapping Assign of two nurses in the same day
    ObjValue trySwapNurse( int weekday, NurseID nurse, NurseID nurse2 ) const;
    ObjValue trySwapNurse( const Move &move ) const;
    // evaluate cost of swapping Assign of two nurses in consecutive days start from weekday
    // and record the selected end of the block into weekday2
    // the recorded move will always be no tabu move or meet aspiration criteria
    ObjValue trySwapBlock( int weekday, int &weekday2, NurseID nurse, NurseID nurse2 ) const;
    ObjValue trySwapBlock( const Move &move ) const;
    // evaluate cost of swapping Assign of two nurses in consecutive days in a week
    // and record the block information into weekday and weekday2
    // the recorded move will always be no tabu move or meet aspiration criteria
    ObjValue trySwapBlock_fast( int &weekday, int &weekday2, NurseID nurse, NurseID nurse2 ) const;
    ObjValue trySwapBlock_fast( const Move &move ) const;
    // evaluate cost of exchanging Assign of a nurse on two days
    ObjValue tryExchangeDay( int weekday, NurseID nurse, int weekday2 ) const;
    ObjValue tryExchangeDay( const Move &move ) const;

    // apply assigning a Assign to nurse without Assign in weekday
    void addAssign( int weekday, NurseID nurse, const Assign &a );
    void addAssign( const Move &move );
    // apply assigning another Assign or skill to nurse already assigned in weekday
    void changeAssign( int weekday, NurseID nurse, const Assign &a );
    void changeAssign( const Move &move );
    // apply removing a Assign to nurse in weekday
    void removeAssign( int weekday, NurseID nurse );
    void removeAssign( const Move &move );
    // apply swapping Assign of two nurses in the same day
    void swapNurse( int weekday, NurseID nurse, NurseID nurse2 );
    void swapNurse( const Move &move );
    // apply swapping Assign of two nurses in consecutive days within [weekday, weekday2]
    void swapBlock( int weekday, int weekday2, NurseID nurse, NurseID nurse2 );
    void swapBlock( const Move &move );
    // apply exchanging Assign of a nurse on two days
    void exchangeDay( int weekday, NurseID nurse, int weekday2 );
    void exchangeDay( const Move &move );

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
    bool noSwapTabu( int weekday, NurseID nurse, NurseID nurse2 ) const
    {
        const Assign &a( assign[nurse][weekday] );
        const Assign &a2( assign[nurse2][weekday] );

        if (a.isWorking()) {
            if (a2.isWorking()) {
                return ((iterCount > shiftTabu[nurse][weekday][a2.shift][a2.skill])
                    || (iterCount > shiftTabu[nurse2][weekday][a.shift][a.skill]));
            } else {
                return ((iterCount > dayTabu[nurse][weekday])
                    || (iterCount > shiftTabu[nurse2][weekday][a.shift][a.skill]));
            }
        } else {
            if (a2.isWorking()) {
                return ((iterCount > shiftTabu[nurse][weekday][a2.shift][a2.skill])
                    || (iterCount > dayTabu[nurse2][weekday]));
            } else {    // no change
                return true;
            }
        }
    }
    bool noSwapTabu( const Move &move ) const
    {
        return noSwapTabu( move.weekday, move.nurse, move.nurse2 );
    }
    bool noExchangeTabu( const Move &move ) const
    {
        const Assign &a( assign[move.nurse][move.weekday] );
        const Assign &a2( assign[move.nurse][move.weekday2] );

        if (a.isWorking()) {
            if (a2.isWorking()) {
                return ((iterCount > shiftTabu[move.nurse][move.weekday][a2.shift][a2.skill])
                    || (iterCount > shiftTabu[move.nurse][move.weekday2][a.shift][a.skill]));
            } else {
                return ((iterCount > dayTabu[move.nurse][move.weekday])
                    || (iterCount > shiftTabu[move.nurse][move.weekday2][a.shift][a.skill]));
            }
        } else {
            if (a2.isWorking()) {
                return ((iterCount > shiftTabu[move.nurse][move.weekday][a2.shift][a2.skill])
                    || (iterCount > dayTabu[move.nurse][move.weekday2]));
            } else {    // no change
                return true;
            }
        }
    }
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
    bool noBlockSwapTabu( int noTabuCount, int totalSwap ) const
    {
#if INRC2_BLOCK_SWAP_TABU_STRENGTH == INRC2_BLOCK_SWAP_AVERAGE_TABU
        return (2 * noTabuCount > totalSwap);   // over half of swaps are no tabu
#elif INRC2_BLOCK_SWAP_TABU_STRENGTH == INRC2_BLOCK_SWAP_STRONG_TABU
        return (noTabuCount == totalSwap);
#elif INRC2_BLOCK_SWAP_TABU_STRENGTH == INRC2_BLOCK_SWAP_WEAK_TABU
        return (noTabuCount > 0);
#endif
    }
#endif

    bool aspirationCritiera( ObjValue bestMoveDelta, ObjValue bestMoveDelta_tabu ) const
    {
        return ((bestMoveDelta >= DefaultPenalty::MAX_OBJ_VALUE)
            || ((objValue + bestMoveDelta_tabu < optima.getObjValue())
            && (bestMoveDelta_tabu < bestMoveDelta)));
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
        const Assign &a2( assign[move.nurse2][move.weekday] );

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
    void updateExchangeTabu( const Move &move )
    {
        const Assign &a( assign[move.nurse][move.weekday] );
        const Assign &a2( assign[move.nurse][move.weekday2] );

        if (a.isWorking()) {
            if (a2.isWorking()) {
                updateShiftTabu( move.nurse, move.weekday, a );
                updateShiftTabu( move.nurse, move.weekday2, a2 );
            } else {
                updateShiftTabu( move.nurse, move.weekday, a );
                updateDayTabu( move.nurse, move.weekday2 );
            }
        } else {
            if (a2.isWorking()) {
                updateDayTabu( move.nurse, move.weekday );
                updateShiftTabu( move.nurse, move.weekday2, a2 );
            }
        }
    }
    void updateBlockSwapTabu( const Move &move )
    {
        Move m( move );
        for (; m.weekday <= m.weekday2; ++m.weekday) {
            updateSwapTabu( move );
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


    mutable Penalty penalty;    // trySwapNurse() will modify it

    // for switching between add and remove
    // 1 for no improvement which is opposite in localSearch
    mutable bool findBestARLoop_flag;    // findBestARLoop() will modify it
    mutable bool findBestARLoopOnBlockBorder_flag;   // findBestARLoopOnBlockBorder() will modify it
    // for controlling start point of the search of best block swap
    mutable NurseID findBestBlockSwap_startNurse;    // findBestBlockSwap() will modify it
    // for controlling swap and block swap will not be selected both in possibility select
    bool isPossibilitySelect;
    mutable bool isBlockSwapSelected;    // tabuSearch_Possibility() and findBestBlockSwap() wil modify it
    // trySwapNurse() and trySwapBlock() will guarantee they are always corresponding to the selected swap
    mutable ObjValue nurseDelta;    // trySwapNurse() and trySwapBlock() will modify it
    mutable ObjValue nurse2Delta;   // trySwapNurse() and trySwapBlock() will modify it

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

    // local optima in the trajectory of current search call
    Output optima;  // set optima to *this before every search

private:    // forbidden operators
    Solution& operator=(const Solution &) { return *this; }
};



#endif
