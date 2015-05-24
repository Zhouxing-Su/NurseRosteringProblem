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
    Output( ObjValue objVal, const AssignTable &assignment, double secondaryObjVal = DefaultPenalty::FORBIDDEN_MOVE )
        : objValue( objVal ), secondaryObjValue( secondaryObjVal ), assign( assignment )
    {
    }

    const Assign& getAssign( NurseID nurse, int weekday ) const
    {
        return assign[nurse][weekday];
    }
    const AssignTable& getAssignTable() const { return assign; }
    ObjValue getObjValue() const { return objValue; }
    double getSecondaryObjValue() const { return secondaryObjValue; }

protected:
    AssignTable assign;
    ObjValue objValue;
    double secondaryObjValue;
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
            Add, Remove, Change, BlockSwap, BASIC_MOVE_SIZE, SIZE = BASIC_MOVE_SIZE
        };

        Move() : delta( DefaultPenalty::FORBIDDEN_MOVE ) {}
        Move( ObjValue d, int w, int w2, NurseID n, NurseID n2 )
            : delta( d ), weekday( w ), weekday2( w2 ), nurse( n ), nurse2( n2 )
        {
        }

        friend bool operator<(const Move &l, const Move &r)
        {
            return (l.delta > r.delta);
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

    typedef void (Solution::*TabuSearch)(const Timer &timer, const FindBestMoveTable &findBestMoveTable, IterCount maxNoImproveCount);


    // entrance in this table should have the same sequence
    // as move mode enum defined in Move::Mode
    static const FindBestMoveTable findBestMove;
    static const FindBestMoveTable findBestMove_repair;
    static const ApplyMoveTable applyMove;
    static const UpdateTabuTable updateTabuTable;

    static const double NO_DIFF;    // for building same assign in rebuild()

    const Solver &solver;
    const NurseRostering &problem;


    Solution( const Solver &solver );
    Solution( const Solver &solver, const Output &output );

    // get local optima in the search trajectory
    const Output& getOptima() const { return optima; }
    // return true if update succeed
    bool updateOptima()
    {
#ifdef INRC2_SECONDARY_OBJ_VALUE
        if (objValue <= optima.getObjValue()) {
            secondaryObjValue = 0;
            for (NurseID n = 0; n < problem.scenario.nurseNum; ++n) {
                const NurseRostering::Scenario::Nurse &nurse( problem.scenario.nurses[n] );
                secondaryObjValue += (static_cast<double>(totalAssignNums[n]) / (1 + abs(
                    nurse.restMaxShiftNum + problem.scenario.contracts[nurse.contract].maxShiftNum )));
            }
        }
#endif
        if (objValue < optima.getObjValue()) {
            optima = *this;
            return true;
        } else if (objValue == optima.getObjValue()) {
#ifdef INRC2_SECONDARY_OBJ_VALUE
            bool isSelected = (secondaryObjValue < optima.getSecondaryObjValue());
#else
            bool isSelected = ((solver.randGen() % 2) == 0);
#endif
            if (isSelected) {
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
    void evaluateObjValue( bool considerSpanningConstraint = true );
    ObjValue evaluateInsufficientStaff();
    ObjValue evaluateConsecutiveShift( NurseID nurse );
    ObjValue evaluateConsecutiveDay( NurseID nurse );
    ObjValue evaluateConsecutiveDayOff( NurseID nurse );
    ObjValue evaluatePreference( NurseID nurse );
    ObjValue evaluateCompleteWeekend( NurseID nurse );
    ObjValue evaluateTotalAssign( NurseID nurse );
    ObjValue evaluateTotalWorkingWeekend( NurseID nurse );
    // get history for next week, only used for custom file
    History genHistory() const;
    // get current iteration count (may not be the actual iter count)
    IterCount getIterCount() const { return iterCount; }

    void genInitAssign();
    bool genInitAssign_Greedy();
    bool repair( const Timer &timer );  // make infeasible solution feasible

    // set weights of nurses with less penalty to 0
    // attention that rebuild() with clear the effect of this method
    void adjustWeightToBiasNurseWithGreaterPenalty( int inverseTotalBiasRatio, int inversePenaltyBiasRatio );


    // select single neighborhood to search in each iteration randomly
    // the random select process is a discrete distribution
    // the possibility to be selected will increase if the neighborhood
    // improve the solution, else decrease it. the sum of possibilities is 1.0
    void tabuSearch_Rand( const Timer &timer, IterCount maxNoImproveCount );


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

    // information for update delta and assignment
    struct BlockSwapCacheItem
    {
    public:
        ObjValue delta;
        int weekday;
        int weekday2;
    };
    // BlockSwapCache[nurse][nurse2] stores
    // delta of best block swap with nurse and nurse2
    typedef std::vector< std::vector< BlockSwapCacheItem > > BlockSwapCache;


    // find day number to be punished for a single block
    // work for shift, day and day-off
    static ObjValue penaltyDayNum( int len, int end, int min, int max )
    {
        return (end < Weekday::Sun) ?
            distanceToRange( len, min, max ) :
            exceedCount( len, max );
    }

    // allocate space for data structure and set default value
    // must be called before building up a solution
    void resetAssign();
    void resetAssistData();

    // reset all cache valid flag of corresponding nurses to false
    void invalidateCacheFlag( const Move &move )
    {
        isBlockSwapCacheValid[move.nurse] = false;
        if (move.mode == Move::Mode::BlockSwap) {
            isBlockSwapCacheValid[move.nurse2] = false;
        }
    }

    // apply the moves defined in Move::Mode as BASIC_MOVE,
    // update objective value and update cache valid flags
    void applyBasicMove( const Move &move )
    {
        (this->*applyMove[move.mode])(move);
        objValue += move.delta;
        invalidateCacheFlag( move );
    }


    // return true if the solution will be improved (delta < 0)
    // BlockBorder means the start or end day of a consecutive block
    bool findBestAdd( Move &bestMove ) const;
    bool findBestChange( Move &bestMove ) const;
    bool findBestRemove( Move &bestMove ) const;
    bool findBestBlockSwap_cached( Move &bestMove ) const;
    bool findBestARBoth_repair( Move &bestMove ) const;
    bool findBestAddOnBlockBorder( Move &bestMove ) const;
    bool findBestChange_repair( Move &bestMove ) const;
    bool findBestARBothOnBlockBorder( Move &bestMove ) const;

    // evaluate cost of adding a Assign to nurse without Assign in weekday
    template <
        const int SingleAssign = DefaultPenalty::FORBIDDEN_MOVE,
        const int UnderStaff = DefaultPenalty::FORBIDDEN_MOVE,
        const int Succession = DefaultPenalty::FORBIDDEN_MOVE,
        const int MissSkill = DefaultPenalty::FORBIDDEN_MOVE,
        const int InsufficientStaff = DefaultPenalty::InsufficientStaff,
        const int ConsecutiveShift = DefaultPenalty::ConsecutiveShift,
        const int ConsecutiveDay = DefaultPenalty::ConsecutiveDay,
        const int ConsecutiveDayOff = DefaultPenalty::ConsecutiveDayOff,
        const int Preference = DefaultPenalty::Preference,
        const int CompleteWeekend = DefaultPenalty::CompleteWeekend,
        const int TotalAssign = DefaultPenalty::TotalAssign,
        const int TotalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend
    >
    ObjValue tryAddAssign( int weekday, NurseID nurse, const Assign &a ) const
    {
        ObjValue delta = 0;

        // TODO : make sure they won't be the same and leave out this
        if (!a.isWorking() || assign.isWorking( nurse, weekday )) {
            return DefaultPenalty::FORBIDDEN_MOVE;
        }

        // hard constraint check
        delta += MissSkill * (!problem.scenario.nurses[nurse].skills[a.skill]);

        delta += Succession * (!isValidSuccession( nurse, a.shift, weekday ));
        delta += Succession * (!isValidPrior( nurse, a.shift, weekday ));

        if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
            return delta;
        }

        const WeekData &weekData( problem.weekData );
        delta -= UnderStaff * (weekData.minNurseNums[weekday][a.shift][a.skill] >
            (weekData.optNurseNums[weekday][a.shift][a.skill] - missingNurseNums[weekday][a.shift][a.skill]));

        int prevDay = weekday - 1;
        int nextDay = weekday + 1;
        ContractID contractID = problem.scenario.nurses[nurse].contract;
        const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
        const Consecutive &c( consecutives[nurse] );

        // insufficient staff
        delta -= InsufficientStaff *
            (missingNurseNums[weekday][a.shift][a.skill] > 0);

        if (nurseWeights[nurse] == 0) {
            return delta;   // TODO : weight ?
        }

        // consecutive shift
        const vector<Scenario::Shift> &shifts( problem.scenario.shifts );
        const Scenario::Shift &shift( shifts[a.shift] );
        ShiftID prevShiftID = assign[nurse][prevDay].shift;
        if (weekday == Weekday::Sun) {  // there is no blocks on the right
            // shiftHigh[weekday] will always be equal to Weekday::Sun
            if ((Weekday::Sun == c.shiftLow[weekday])
                && (a.shift == prevShiftID)) {
                const Scenario::Shift &prevShift( shifts[prevShiftID] );
                delta -= ConsecutiveShift *
                    distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sat],
                    prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                delta += ConsecutiveShift * exceedCount(
                    Weekday::Sun - c.shiftLow[Weekday::Sat] + 1, shift.maxConsecutiveShiftNum );
            } else {    // have nothing to do with previous block
                delta += ConsecutiveShift *    // penalty on day off is counted later
                    exceedCount( 1, shift.maxConsecutiveShiftNum );
            }
        } else {
            ShiftID nextShiftID = assign[nurse][nextDay].shift;
            if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
                int high = weekday;
                int low = weekday;
                if (prevShiftID == a.shift) {
                    const Scenario::Shift &prevShift( shifts[prevShiftID] );
                    low = c.shiftLow[prevDay];
                    delta -= ConsecutiveShift *
                        distanceToRange( weekday - c.shiftLow[prevDay],
                        prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                }
                if (nextShiftID == a.shift) {
                    const Scenario::Shift &nextShift( shifts[nextShiftID] );
                    high = c.shiftHigh[nextDay];
                    delta -= ConsecutiveShift * penaltyDayNum(
                        c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                        nextShift.minConsecutiveShiftNum, nextShift.maxConsecutiveShiftNum );
                }
                delta += ConsecutiveShift * penaltyDayNum( high - low + 1, high,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            } else if (weekday == c.shiftHigh[weekday]) {
                if (a.shift == nextShiftID) {
                    const Scenario::Shift &nextShift( shifts[nextShiftID] );
                    int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                    if (consecutiveShiftOfNextBlock >= nextShift.maxConsecutiveShiftNum) {
                        delta += ConsecutiveShift;
                    } else if ((c.shiftHigh[nextDay] < Weekday::Sun)
                        && (consecutiveShiftOfNextBlock < nextShift.minConsecutiveShiftNum)) {
                        delta -= ConsecutiveShift;
                    }
                } else {
                    delta += ConsecutiveShift * distanceToRange( 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
            } else if (weekday == c.shiftLow[weekday]) {
                if (a.shift == prevShiftID) {
                    const Scenario::Shift &prevShift( shifts[prevShiftID] );
                    int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                    if (consecutiveShiftOfPrevBlock >= prevShift.maxConsecutiveShiftNum) {
                        delta += ConsecutiveShift;
                    } else if (consecutiveShiftOfPrevBlock < prevShift.minConsecutiveShiftNum) {
                        delta -= ConsecutiveShift;
                    }
                } else {
                    delta += ConsecutiveShift * distanceToRange( 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
            } else {
                delta += ConsecutiveShift * distanceToRange( 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            }
        }

        // consecutive day and day-off
        if (weekday == Weekday::Sun) {  // there is no block on the right
            // dayHigh[weekday] will always be equal to Weekday::Sun
            if (Weekday::Sun == c.dayLow[weekday]) {
                delta -= ConsecutiveDay *
                    distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta -= ConsecutiveDayOff *
                    exceedCount( 1, contract.maxConsecutiveDayoffNum );
                delta += ConsecutiveDay * exceedCount(
                    Weekday::Sun - c.dayLow[Weekday::Sat] + 1, contract.maxConsecutiveDayNum );
            } else {    // day off block length over 1
                delta -= ConsecutiveDayOff * exceedCount(
                    Weekday::Sun - c.dayLow[Weekday::Sun] + 1, contract.maxConsecutiveDayoffNum );
                delta += ConsecutiveDayOff * distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sun],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta += ConsecutiveDay * exceedCount( 1, contract.maxConsecutiveDayNum );
            }
        } else {
            if (c.dayHigh[weekday] == c.dayLow[weekday]) {
                delta -= ConsecutiveDay * distanceToRange( weekday - c.dayLow[prevDay],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta -= ConsecutiveDayOff * distanceToRange( 1,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta -= ConsecutiveDay * penaltyDayNum(
                    c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta += ConsecutiveDay * penaltyDayNum(
                    c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            } else if (weekday == c.dayHigh[weekday]) {
                int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
                if (consecutiveDayOfNextBlock >= contract.maxConsecutiveDayNum) {
                    delta += ConsecutiveDay;
                } else if ((c.dayHigh[nextDay] < Weekday::Sun)
                    && (consecutiveDayOfNextBlock < contract.minConsecutiveDayNum)) {
                    delta -= ConsecutiveDay;
                }
                int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
                if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                    delta -= ConsecutiveDayOff;
                } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                    delta += ConsecutiveDayOff;
                }
            } else if (weekday == c.dayLow[weekday]) {
                int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
                if (consecutiveDayOfPrevBlock >= contract.maxConsecutiveDayNum) {
                    delta += ConsecutiveDay;
                } else if (consecutiveDayOfPrevBlock < contract.minConsecutiveDayNum) {
                    delta -= ConsecutiveDay;
                }
                int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
                if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                    delta -= ConsecutiveDayOff;
                } else if ((c.dayHigh[weekday] < Weekday::Sun)
                    && (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum)) {
                    delta += ConsecutiveDayOff;
                }
            } else {
                delta -= ConsecutiveDayOff * penaltyDayNum(
                    c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta += ConsecutiveDayOff *
                    distanceToRange( weekday - c.dayLow[weekday],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta += ConsecutiveDay * distanceToRange( 1,
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta += ConsecutiveDayOff * penaltyDayNum(
                    c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            }
        }

        // preference
        delta += Preference *
            weekData.shiftOffs[weekday][a.shift][nurse];

        if (weekday > Weekday::Fri) {
            int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
            // complete weekend
            if (contract.completeWeekend) {
                if (assign.isWorking( nurse, theOtherDay )) {
                    delta -= CompleteWeekend;
                } else {
                    delta += CompleteWeekend;
                }
            }

            // total working weekend
            if (!assign.isWorking( nurse, theOtherDay )) {
#ifdef INRC2_AVERAGE_MAX_WORKING_WEEKEND
                const History &history( problem.history );
                int currentWeek = history.currentWeek;
                delta -= TotalWorkingWeekend * exceedCount(
                    history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum,
                    contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
                delta += TotalWorkingWeekend * exceedCount(
                    (history.totalWorkingWeekendNums[nurse] + 1) * problem.scenario.totalWeekNum,
                    contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
#else
                delta -= TotalWorkingWeekend * exceedCount( 0,
                    problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
                delta += TotalWorkingWeekend * exceedCount( problem.history.restWeekCount,
                    problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
#endif
            }
        }

        // total assign (expand problem.history.restWeekCount times)
        int restMinShift = problem.scenario.nurses[nurse].restMinShiftNum;
        int restMaxShift = problem.scenario.nurses[nurse].restMaxShiftNum;
        int totalAssign = totalAssignNums[nurse] * problem.history.restWeekCount;
        delta -= TotalAssign * distanceToRange( totalAssign,
            restMinShift, restMaxShift ) / problem.history.restWeekCount;
        delta += TotalAssign * distanceToRange( totalAssign + problem.history.restWeekCount,
            restMinShift, restMaxShift ) / problem.history.restWeekCount;

        return delta;   // TODO : weight ?
    }
    ObjValue tryAddAssign_blockSwap( int weekday, NurseID nurse, const Assign &a ) const
    {
        return tryAddAssign < 0, 0, 0, 0, 0,
            DefaultPenalty::ConsecutiveShift,
            DefaultPenalty::ConsecutiveDay,
            DefaultPenalty::ConsecutiveDayOff,
            DefaultPenalty::Preference,
            DefaultPenalty::CompleteWeekend,
            DefaultPenalty::TotalAssign,
            DefaultPenalty::TotalWorkingWeekend > ( weekday, nurse, a );
    }
    ObjValue tryAddAssign_repair( int weekday, NurseID nurse, const Assign &a ) const
    {
        return tryAddAssign <
            DefaultPenalty::FORBIDDEN_MOVE,
            DefaultPenalty::UnderStaff_Repair,
            DefaultPenalty::Succession_Repair,
            DefaultPenalty::FORBIDDEN_MOVE,
            0, 0, 0, 0, 0, 0, 0, 0 > ( weekday, nurse, a );
    }
    // evaluate cost of assigning another Assign or skill to nurse already assigned in weekday
    template <
        const int SingleAssign = DefaultPenalty::FORBIDDEN_MOVE,
        const int UnderStaff = DefaultPenalty::FORBIDDEN_MOVE,
        const int Succession = DefaultPenalty::FORBIDDEN_MOVE,
        const int MissSkill = DefaultPenalty::FORBIDDEN_MOVE,
        const int InsufficientStaff = DefaultPenalty::InsufficientStaff,
        const int ConsecutiveShift = DefaultPenalty::ConsecutiveShift,
        const int ConsecutiveDay = DefaultPenalty::ConsecutiveDay,
        const int ConsecutiveDayOff = DefaultPenalty::ConsecutiveDayOff,
        const int Preference = DefaultPenalty::Preference,
        const int CompleteWeekend = DefaultPenalty::CompleteWeekend,
        const int TotalAssign = DefaultPenalty::TotalAssign,
        const int TotalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend
    >
    ObjValue tryChangeAssign( int weekday, NurseID nurse, const Assign &a ) const
    {
        ObjValue delta = 0;

        ShiftID oldShiftID = assign[nurse][weekday].shift;
        SkillID oldSkillID = assign[nurse][weekday].skill;
        // TODO : make sure they won't be the same and leave out this
        if (!a.isWorking() || !Assign::isWorking( oldShiftID )
            || ((a.shift == oldShiftID) && (a.skill == oldSkillID))) {
            return DefaultPenalty::FORBIDDEN_MOVE;
        }

        delta += MissSkill * (!problem.scenario.nurses[nurse].skills[a.skill]);

        delta += Succession * (!isValidSuccession( nurse, a.shift, weekday ));
        delta += Succession * (!isValidPrior( nurse, a.shift, weekday ));

        const WeekData &weekData( problem.weekData );
        delta += UnderStaff * (weekData.minNurseNums[weekday][oldShiftID][oldSkillID] >=
            (weekData.optNurseNums[weekday][oldShiftID][oldSkillID] - missingNurseNums[weekday][oldShiftID][oldSkillID]));

        if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
            return delta;
        }

        delta -= Succession * (!isValidSuccession( nurse, oldShiftID, weekday ));
        delta -= Succession * (!isValidPrior( nurse, oldShiftID, weekday ));

        delta -= UnderStaff * (weekData.minNurseNums[weekday][a.shift][a.skill] >
            (weekData.optNurseNums[weekday][a.shift][a.skill] - missingNurseNums[weekday][a.shift][a.skill]));

        int prevDay = weekday - 1;
        int nextDay = weekday + 1;
        const Consecutive &c( consecutives[nurse] );

        // insufficient staff
        delta += InsufficientStaff *
            (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);
        delta -= InsufficientStaff *
            (missingNurseNums[weekday][a.shift][a.skill] > 0);

        if (nurseWeights[nurse] == 0) {
            return delta;   // TODO : weight ?
        }

        if (a.shift != oldShiftID) {
            // consecutive shift
            const vector<Scenario::Shift> &shifts( problem.scenario.shifts );
            const Scenario::Shift &shift( shifts[a.shift] );
            const Scenario::Shift &oldShift( problem.scenario.shifts[oldShiftID] );
            ShiftID prevShiftID = assign[nurse][prevDay].shift;
            if (weekday == Weekday::Sun) {  // there is no blocks on the right
                // shiftHigh[weekday] will always equal to Weekday::Sun
                if (Weekday::Sun == c.shiftLow[weekday]) {
                    if (a.shift == prevShiftID) {
                        const Scenario::Shift &prevShift( shifts[prevShiftID] );
                        delta -= ConsecutiveShift *
                            distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sat],
                            prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                        delta -= ConsecutiveShift *
                            exceedCount( 1, oldShift.maxConsecutiveShiftNum );
                        delta += ConsecutiveShift * exceedCount(
                            Weekday::Sun - c.shiftLow[Weekday::Sat] + 1, shift.maxConsecutiveShiftNum );
                    } else {
                        delta -= ConsecutiveShift *
                            exceedCount( 1, oldShift.maxConsecutiveShiftNum );
                        delta += ConsecutiveShift *
                            exceedCount( 1, shift.maxConsecutiveShiftNum );
                    }
                } else {    // block length over 1
                    delta -= ConsecutiveShift * exceedCount(
                        Weekday::Sun - c.shiftLow[Weekday::Sun] + 1, oldShift.maxConsecutiveShiftNum );
                    delta += ConsecutiveShift * distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sun],
                        oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                    delta += ConsecutiveShift *
                        exceedCount( 1, shift.maxConsecutiveShiftNum );
                }
            } else {
                ShiftID nextShiftID = assign[nurse][nextDay].shift;
                if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
                    int high = weekday;
                    int low = weekday;
                    if (prevShiftID == a.shift) {
                        const Scenario::Shift &prevShift( shifts[prevShiftID] );
                        low = c.shiftLow[prevDay];
                        delta -= ConsecutiveShift *
                            distanceToRange( weekday - c.shiftLow[prevDay],
                            prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                    }
                    if (nextShiftID == a.shift) {
                        const Scenario::Shift &nextShift( shifts[nextShiftID] );
                        high = c.shiftHigh[nextDay];
                        delta -= ConsecutiveShift * penaltyDayNum(
                            c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                            nextShift.minConsecutiveShiftNum, nextShift.maxConsecutiveShiftNum );
                    }
                    delta -= ConsecutiveShift * distanceToRange( 1,
                        oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                    delta += ConsecutiveShift * penaltyDayNum( high - low + 1, high,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                } else if (weekday == c.shiftHigh[weekday]) {
                    if (nextShiftID == a.shift) {
                        const Scenario::Shift &nextShift( shifts[nextShiftID] );
                        int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                        if (consecutiveShiftOfNextBlock >= nextShift.maxConsecutiveShiftNum) {
                            delta += ConsecutiveShift;
                        } else if ((c.shiftHigh[nextDay] < Weekday::Sun)
                            && (consecutiveShiftOfNextBlock < nextShift.minConsecutiveShiftNum)) {
                            delta -= ConsecutiveShift;
                        }
                    } else {
                        delta += ConsecutiveShift * distanceToRange( 1,
                            shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    }
                    int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
                    if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                        delta -= ConsecutiveShift;
                    } else if (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum) {
                        delta += ConsecutiveShift;
                    }
                } else if (weekday == c.shiftLow[weekday]) {
                    if (prevShiftID == a.shift) {
                        const Scenario::Shift &prevShift( shifts[prevShiftID] );
                        int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                        if (consecutiveShiftOfPrevBlock >= prevShift.maxConsecutiveShiftNum) {
                            delta += ConsecutiveShift;
                        } else if (consecutiveShiftOfPrevBlock < prevShift.minConsecutiveShiftNum) {
                            delta -= ConsecutiveShift;
                        }
                    } else {
                        delta += ConsecutiveShift * distanceToRange( 1,
                            shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    }
                    int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
                    if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                        delta -= ConsecutiveShift;
                    } else if ((c.shiftHigh[weekday] < Weekday::Sun)
                        && (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum)) {
                        delta += ConsecutiveShift;
                    }
                } else {
                    delta -= ConsecutiveShift * penaltyDayNum(
                        c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                        oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                    delta += ConsecutiveShift *
                        distanceToRange( weekday - c.shiftLow[weekday],
                        oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                    delta += ConsecutiveShift * distanceToRange( 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    delta += ConsecutiveShift *
                        penaltyDayNum( c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                        oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                }
            }

            // preference
            delta += Preference *
                weekData.shiftOffs[weekday][a.shift][nurse];
            delta -= Preference *
                weekData.shiftOffs[weekday][oldShiftID][nurse];
        }

        return delta;   // TODO : weight ?
    }
    ObjValue tryChangeAssign_blockSwap( int weekday, NurseID nurse, const Assign &a ) const
    {
        return tryChangeAssign < 0, 0, 0, 0, 0,
            DefaultPenalty::ConsecutiveShift,
            DefaultPenalty::ConsecutiveDay,
            DefaultPenalty::ConsecutiveDayOff,
            DefaultPenalty::Preference,
            DefaultPenalty::CompleteWeekend,
            DefaultPenalty::TotalAssign,
            DefaultPenalty::TotalWorkingWeekend > ( weekday, nurse, a );
    }
    ObjValue tryChangeAssign_repair( int weekday, NurseID nurse, const Assign &a ) const
    {
        return tryChangeAssign <
            DefaultPenalty::FORBIDDEN_MOVE,
            DefaultPenalty::UnderStaff_Repair,
            DefaultPenalty::Succession_Repair,
            DefaultPenalty::FORBIDDEN_MOVE,
            0, 0, 0, 0, 0, 0, 0, 0 > ( weekday, nurse, a );
    }
    // evaluate cost of removing the Assign from nurse already assigned in weekday
    template <
        const int SingleAssign = DefaultPenalty::FORBIDDEN_MOVE,
        const int UnderStaff = DefaultPenalty::FORBIDDEN_MOVE,
        const int Succession = DefaultPenalty::FORBIDDEN_MOVE,
        const int MissSkill = DefaultPenalty::FORBIDDEN_MOVE,
        const int InsufficientStaff = DefaultPenalty::InsufficientStaff,
        const int ConsecutiveShift = DefaultPenalty::ConsecutiveShift,
        const int ConsecutiveDay = DefaultPenalty::ConsecutiveDay,
        const int ConsecutiveDayOff = DefaultPenalty::ConsecutiveDayOff,
        const int Preference = DefaultPenalty::Preference,
        const int CompleteWeekend = DefaultPenalty::CompleteWeekend,
        const int TotalAssign = DefaultPenalty::TotalAssign,
        const int TotalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend
    >
    ObjValue tryRemoveAssign( int weekday, NurseID nurse ) const
    {
        ObjValue delta = 0;

        ShiftID oldShiftID = assign[nurse][weekday].shift;
        SkillID oldSkillID = assign[nurse][weekday].skill;
        // TODO : make sure they won't be the same and leave out this
        if (!Assign::isWorking( oldShiftID )) {
            return DefaultPenalty::FORBIDDEN_MOVE;
        }

        const WeekData &weekData( problem.weekData );
        delta += UnderStaff * (weekData.minNurseNums[weekday][oldShiftID][oldSkillID] >=
            (weekData.optNurseNums[weekday][oldShiftID][oldSkillID] - missingNurseNums[weekday][oldShiftID][oldSkillID]));

        if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
            return delta;
        }

        delta -= Succession * (!isValidSuccession( nurse, oldShiftID, weekday ));
        delta -= Succession * (!isValidPrior( nurse, oldShiftID, weekday ));

        int prevDay = weekday - 1;
        int nextDay = weekday + 1;
        ContractID contractID = problem.scenario.nurses[nurse].contract;
        const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
        const Consecutive &c( consecutives[nurse] );

        // insufficient staff
        delta += InsufficientStaff *
            (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);

        if (nurseWeights[nurse] == 0) {
            return delta;   // TODO : weight ?
        }

        // consecutive shift
        const vector<Scenario::Shift> &shifts( problem.scenario.shifts );
        const Scenario::Shift &oldShift( shifts[oldShiftID] );
        if (weekday == Weekday::Sun) {  // there is no block on the right
            if (Weekday::Sun == c.shiftLow[weekday]) {
                delta -= ConsecutiveShift * exceedCount( 1, oldShift.maxConsecutiveShiftNum );
            } else {
                delta -= ConsecutiveShift * exceedCount(
                    Weekday::Sun - c.shiftLow[weekday] + 1, oldShift.maxConsecutiveShiftNum );
                delta += ConsecutiveShift * distanceToRange( Weekday::Sun - c.shiftLow[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            }
        } else {
            if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
                delta -= ConsecutiveShift * distanceToRange( 1,
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            } else if (weekday == c.shiftHigh[weekday]) {
                int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
                if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                    delta -= ConsecutiveShift;
                } else if (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum) {
                    delta += ConsecutiveShift;
                }
            } else if (weekday == c.shiftLow[weekday]) {
                int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
                if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                    delta -= ConsecutiveShift;
                } else if ((c.shiftHigh[weekday] < Weekday::Sun)
                    && (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum)) {
                    delta += ConsecutiveShift;
                }
            } else {
                delta -= ConsecutiveShift * penaltyDayNum(
                    c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += ConsecutiveShift * distanceToRange( weekday - c.shiftLow[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += ConsecutiveShift * penaltyDayNum(
                    c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            }
        }

        // consecutive day and day-off
        if (weekday == Weekday::Sun) {  // there is no blocks on the right
            // dayHigh[weekday] will always be equal to Weekday::Sun
            if (Weekday::Sun == c.dayLow[weekday]) {
                delta -= ConsecutiveDayOff *
                    distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta -= ConsecutiveDay *
                    exceedCount( 1, contract.maxConsecutiveDayNum );
                delta += ConsecutiveDayOff * exceedCount(
                    Weekday::Sun - c.dayLow[Weekday::Sat] + 1, contract.maxConsecutiveDayoffNum );
            } else {    // day off block length over 1
                delta -= ConsecutiveDay * exceedCount(
                    Weekday::Sun - c.dayLow[Weekday::Sun] + 1, contract.maxConsecutiveDayNum );
                delta += ConsecutiveDay * distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sun],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta += ConsecutiveDayOff * exceedCount( 1, contract.maxConsecutiveDayoffNum );
            }
        } else {
            if (c.dayHigh[weekday] == c.dayLow[weekday]) {
                delta -= ConsecutiveDayOff *
                    distanceToRange( weekday - c.dayLow[prevDay],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta -= ConsecutiveDay *
                    distanceToRange( 1,
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta -= ConsecutiveDayOff * penaltyDayNum(
                    c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta += ConsecutiveDayOff * penaltyDayNum(
                    c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            } else if (weekday == c.dayHigh[weekday]) {
                int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
                if (consecutiveDayOfNextBlock >= contract.maxConsecutiveDayoffNum) {
                    delta += ConsecutiveDayOff;
                } else if ((c.dayHigh[nextDay] < Weekday::Sun)
                    && (consecutiveDayOfNextBlock < contract.minConsecutiveDayoffNum)) {
                    delta -= ConsecutiveDayOff;
                }
                int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
                if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayNum) {
                    delta -= ConsecutiveDay;
                } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayNum) {
                    delta += ConsecutiveDay;
                }
            } else if (weekday == c.dayLow[weekday]) {
                int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
                if (consecutiveDayOfPrevBlock >= contract.maxConsecutiveDayoffNum) {
                    delta += ConsecutiveDayOff;
                } else if (consecutiveDayOfPrevBlock < contract.minConsecutiveDayoffNum) {
                    delta -= ConsecutiveDayOff;
                }
                int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
                if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayNum) {
                    delta -= ConsecutiveDay;
                } else if ((c.dayHigh[weekday] < Weekday::Sun)
                    && (consecutiveDayOfThisBlock <= contract.minConsecutiveDayNum)) {
                    delta += ConsecutiveDay;
                }
            } else {
                delta -= ConsecutiveDay * penaltyDayNum(
                    c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta += ConsecutiveDay *
                    distanceToRange( weekday - c.dayLow[weekday],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                delta += ConsecutiveDayOff * distanceToRange( 1,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta += ConsecutiveDay *
                    penaltyDayNum( c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            }
        }

        // preference
        delta -= Preference *
            weekData.shiftOffs[weekday][oldShiftID][nurse];

        if (weekday > Weekday::Fri) {
            int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
            // complete weekend
            if (contract.completeWeekend) {
                if (assign.isWorking( nurse, theOtherDay )) {
                    delta += CompleteWeekend;
                } else {
                    delta -= CompleteWeekend;
                }
            }

            // total working weekend
            if (!assign.isWorking( nurse, theOtherDay )) {
#ifdef INRC2_AVERAGE_MAX_WORKING_WEEKEND
                const History &history( problem.history );
                int currentWeek = history.currentWeek;
                delta -= TotalWorkingWeekend * exceedCount(
                    (history.totalWorkingWeekendNums[nurse] + 1) * problem.scenario.totalWeekNum,
                    contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
                delta += TotalWorkingWeekend * exceedCount(
                    history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum,
                    contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
#else
                delta -= TotalWorkingWeekend * exceedCount( problem.history.restWeekCount,
                    problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
                delta += TotalWorkingWeekend * exceedCount( 0,
                    problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
#endif
            }
        }

        // total assign (expand problem.history.restWeekCount times)
        int restMinShift = problem.scenario.nurses[nurse].restMinShiftNum;
        int restMaxShift = problem.scenario.nurses[nurse].restMaxShiftNum;
        int totalAssign = totalAssignNums[nurse] * problem.history.restWeekCount;
        delta -= TotalAssign * distanceToRange( totalAssign,
            restMinShift, restMaxShift ) / problem.history.restWeekCount;
        delta += TotalAssign * distanceToRange( totalAssign - problem.history.restWeekCount,
            restMinShift, restMaxShift ) / problem.history.restWeekCount;

        return delta;   // TODO : weight ?
    }
    ObjValue tryRemoveAssign_blockSwap( int weekday, NurseID nurse ) const
    {
        return tryRemoveAssign < 0, 0, 0, 0, 0,
            DefaultPenalty::ConsecutiveShift,
            DefaultPenalty::ConsecutiveDay,
            DefaultPenalty::ConsecutiveDayOff,
            DefaultPenalty::Preference,
            DefaultPenalty::CompleteWeekend,
            DefaultPenalty::TotalAssign,
            DefaultPenalty::TotalWorkingWeekend > ( weekday, nurse );
    }
    ObjValue tryRemoveAssign_repair( int weekday, NurseID nurse ) const
    {
        return tryRemoveAssign <
            DefaultPenalty::FORBIDDEN_MOVE,
            DefaultPenalty::UnderStaff_Repair,
            DefaultPenalty::Succession_Repair,
            DefaultPenalty::FORBIDDEN_MOVE,
            0, 0, 0, 0, 0, 0, 0, 0 > ( weekday, nurse );
    }
    // evaluate cost of swapping Assign of two nurses in the same day
    ObjValue trySwapNurse( int weekday, NurseID nurse, NurseID nurse2 ) const;
    // evaluate cost of swapping Assign of two nurses in consecutive days start from weekday
    // and record the selected end of the block into weekday2
    // the recorded move will always be no tabu move or meet aspiration criteria
    ObjValue trySwapBlock( int weekday, int &weekday2, NurseID nurse, NurseID nurse2 ) const;

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

    void updateConsecutive( int weekday, NurseID nurse, ShiftID shift );
    // the assignment is on the right side of a consecutive block
    void assignHigh( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight );
    // the assignment is on the left side of a consecutive block
    void assignLow( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectLeft );
    // the assignment is in the middle of a consecutive block
    void assignMiddle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE] );
    // the assignment is on a consecutive block with single slot
    void assignSingle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight, bool affectLeft );

#ifdef INRC2_USE_TABU
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
#endif


    // trySwapNurse() and trySwapBlock() will guarantee they are always corresponding to the selected swap
    mutable ObjValue nurseDelta;    // trySwapNurse() and trySwapBlock() will modify it
    mutable ObjValue nurse2Delta;   // trySwapNurse() and trySwapBlock() will modify it

#ifdef INRC2_USE_TABU
    ShiftTabu shiftTabu;
    DayTabu dayTabu;
#endif
    IterCount iterCount;

    // total assignments for each nurse
    std::vector<int> totalAssignNums;
    // missing nurse numbers for each single assignment
    NurseNumsOnSingleAssign missingNurseNums;
    // consecutive[nurse] is the consecutive assignments record for nurse
    std::vector<Consecutive> consecutives;

    // control penalty calculation on each nurse
    std::vector<ObjValue> nurseWeights;

    // rebuild() and weight adjustment will invalidate all items
    mutable BlockSwapCache blockSwapCache;
    // (blockSwapDeltaCacheValidFlag[nurse] == false) means 
    // the delta related to nurse can not be reused
    mutable std::vector<bool> isBlockSwapCacheValid;

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
