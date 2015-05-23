#include "Solution.h"
#include "Solver.h"


using namespace std;


const NurseRostering::Solution::TryMoveTable
NurseRostering::Solution::tryMove = {
    &NurseRostering::Solution::tryAddAssign,
    &NurseRostering::Solution::tryRemoveAssign,
    &NurseRostering::Solution::tryChangeAssign,
    &NurseRostering::Solution::trySwapBlock,
};
const NurseRostering::Solution::FindBestMoveTable
NurseRostering::Solution::findBestMove = {
    &NurseRostering::Solution::findBestAdd,
    &NurseRostering::Solution::findBestRemove,
    &NurseRostering::Solution::findBestChange,
    &NurseRostering::Solution::findBestBlockSwap_cached
};
const NurseRostering::Solution::ApplyMoveTable
NurseRostering::Solution::applyMove = {
    &NurseRostering::Solution::addAssign,
    &NurseRostering::Solution::removeAssign,
    &NurseRostering::Solution::changeAssign,
    &NurseRostering::Solution::swapBlock
};
#ifdef INRC2_USE_TABU
const NurseRostering::Solution::UpdateTabuTable
NurseRostering::Solution::updateTabuTable = {
    &NurseRostering::Solution::updateAddTabu,
    &NurseRostering::Solution::updateRemoveTabu,
    &NurseRostering::Solution::updateChangeTabu,
    &NurseRostering::Solution::updateBlockSwapTabu
};
#endif

const double NurseRostering::Solution::NO_DIFF = -1;



NurseRostering::Solution::Solution( const Solver &s )
    : solver( s ), problem( s.problem ), iterCount( 1 )
{
}

NurseRostering::Solution::Solution( const Solver &s, const Output &output )
    : solver( s ), problem( s.problem ), iterCount( 1 )
{
    rebuild( output );
}

void NurseRostering::Solution::rebuild( const Output &output, double diff )
{
    if (diff < 1) { // greater than 1 means totally change
        unsigned selectBound = static_cast<unsigned>(diff *
            (solver.randGen.max() - solver.randGen.min()) + solver.randGen.min());

        const AssignTable &assignTable( (&output.getAssignTable() != &assign)
            ? output.getAssignTable() : AssignTable( output.getAssignTable() ) );

        resetAssign();
        resetAssistData();

        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
                if (assignTable[nurse][weekday].isWorking()) {
                    if (solver.randGen() >= selectBound) {
                        addAssign( weekday, nurse, assignTable[nurse][weekday] );
                    }
                }
            }
        }
    }

    if (diff > 0) {  // there may be difference
        repair( solver.timer );
    }

    evaluateObjValue();
}

void NurseRostering::Solution::rebuild( const Output &output )
{
    const AssignTable &assignTable( (&output.getAssignTable() != &assign)
        ? output.getAssignTable() : AssignTable( output.getAssignTable() ) );

    resetAssign();
    resetAssistData();

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            if (assignTable[nurse][weekday].isWorking()) {
                addAssign( weekday, nurse, assignTable[nurse][weekday] );
            }
        }
    }

    objValue = output.getObjValue();
}

void NurseRostering::Solution::rebuild()
{
    rebuild( *this, NO_DIFF );
}

void NurseRostering::Solution::genInitAssign()
{
    genInitAssign_Greedy();
    repair( solver.timer );
}

bool NurseRostering::Solution::genInitAssign_Greedy()
{
    resetAssign();
    resetAssistData();

    AvailableNurses availableNurse( *this );
    const NurseNumOfSkill &nurseNumOfSkill( solver.getNurseNumOfSkill() );

    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        // decide assign sequence of skill
        // the greater requiredNurseNum/nurseNumOfSkill[skill] is, the smaller index in skillRank a skill will get
        vector<SkillID> skillRank( problem.scenario.skillTypeNum );
        vector<double> dailyRequire( problem.scenario.skillSize, 0 );
        for (int rank = 0; rank < problem.scenario.skillTypeNum; ++rank) {
            SkillID skill = rank + NurseRostering::Scenario::Skill::ID_BEGIN;
            skillRank[rank] = skill;
            for (ShiftID shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                shift < problem.scenario.shiftSize; ++shift) {
                dailyRequire[skill] += problem.weekData.minNurseNums[weekday][shift][skill];
            }
            dailyRequire[skill] /= nurseNumOfSkill[skill];
        }

        class CmpDailyRequire
        {
        public:
            CmpDailyRequire( vector<double> &dr ) : dailyRequire( dr ) {}

            bool operator()( const int &l, const int &r )
            {
                return (dailyRequire[l] > dailyRequire[r]);
            }
        private:
            const vector<double> &dailyRequire;

        private:    // forbidden operators
            CmpDailyRequire& operator=(const CmpDailyRequire &) { return *this; }
        };
        sort( skillRank.begin(), skillRank.end(), CmpDailyRequire( dailyRequire ) );

        // start assigning nurses
        for (int rank = 0; rank < problem.scenario.skillTypeNum; ++rank) {
            SkillID skill = skillRank[rank];
            availableNurse.setEnvironment( weekday, skill );
            for (ShiftID shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                shift < problem.scenario.shiftSize; ++shift) {
                availableNurse.setShift( shift );
                for (int i = 0; i < problem.weekData.minNurseNums[weekday][shift][skill]; ++i) {
                    int nurse = availableNurse.getNurse();
                    if (nurse != NurseRostering::Scenario::Nurse::ID_NONE) {
                        addAssign( weekday, nurse, Assign( shift, skill ) );
                    } else {
                        return false;
                    }
                }
            }
        }
    }

    evaluateObjValue();
    return true;
}

void NurseRostering::Solution::resetAssign()
{
    assign = AssignTable( problem.scenario.nurseNum, Weekday::SIZE );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        assign[nurse][Weekday::HIS] = Assign( problem.history.lastShifts[nurse] );
        assign[nurse][Weekday::NEXT_WEEK] = Assign();
    }
}

void NurseRostering::Solution::resetAssistData()
{
    // assist data structure
    missingNurseNums = problem.weekData.optNurseNums;
    totalAssignNums = vector<int>( problem.scenario.nurseNum, 0 );
    consecutives = vector<Consecutive>( problem.scenario.nurseNum );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        consecutives[nurse] = Consecutive( problem.history, nurse );
    }
    // penalty weight
    nurseWeights = vector<ObjValue>( problem.scenario.nurseNum, 1 );
#ifdef INRC2_USE_TABU
    // tabu table
    if (shiftTabu.empty() || dayTabu.empty()) {
        shiftTabu = ShiftTabu( problem.scenario.nurseNum,
            vector< vector< vector<IterCount> > >( Weekday::SIZE,
            vector< vector<IterCount> >( problem.scenario.shiftSize,
            vector<IterCount>( problem.scenario.skillSize, 0 ) ) ) );
        dayTabu = DayTabu( problem.scenario.nurseNum,
            vector<IterCount>( Weekday::SIZE, 0 ) );
    } else {
        iterCount += solver.ShiftTabuTenureBase() + solver.ShiftTabuTenureAmp()
            + solver.DayTabuTenureBase() + solver.DayTabuTenureAmp();
    }
#endif
    // delta cache
    if (blockSwapCache.empty()) {
        blockSwapCache = BlockSwapCache( problem.scenario.nurseNum,
            vector<BlockSwapCacheItem>( problem.scenario.nurseNum ) );
    }
    isBlockSwapCacheValid = vector<bool>( problem.scenario.nurseNum, false );
}

void NurseRostering::Solution::evaluateObjValue( bool considerSpanningConstraint )
{
    objInsufficientStaff = evaluateInsufficientStaff();
    objConsecutiveShift = 0;
    objConsecutiveDay = 0;
    objConsecutiveDayOff = 0;
    objPreference = 0;
    objCompleteWeekend = 0;
    objTotalAssign = 0;
    objTotalWorkingWeekend = 0;

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        objConsecutiveShift += evaluateConsecutiveShift( nurse );
        objConsecutiveDay += evaluateConsecutiveDay( nurse );
        objConsecutiveDayOff += evaluateConsecutiveDayOff( nurse );
        objPreference += evaluatePreference( nurse );
        objCompleteWeekend += evaluateCompleteWeekend( nurse );
        if (considerSpanningConstraint) {
            objTotalAssign += evaluateTotalAssign( nurse );
            objTotalWorkingWeekend += evaluateTotalWorkingWeekend( nurse );
        }
    }

    objValue = objInsufficientStaff + objConsecutiveShift + objConsecutiveDay + objConsecutiveDayOff
        + objPreference + objCompleteWeekend + objTotalAssign + objTotalWorkingWeekend;
}

NurseRostering::History NurseRostering::Solution::genHistory() const
{
    const History &history( problem.history );
    History newHistory;
    newHistory.lastShifts.resize( problem.scenario.nurseNum );
    newHistory.consecutiveShiftNums.resize( problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayNums.resize( problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayoffNums.resize( problem.scenario.nurseNum, 0 );

    newHistory.accObjValue = history.accObjValue + objValue;
    newHistory.pastWeekCount = history.currentWeek;
    newHistory.currentWeek = history.currentWeek + 1;
    newHistory.restWeekCount = history.restWeekCount - 1;
    newHistory.totalAssignNums = history.totalAssignNums;
    newHistory.totalWorkingWeekendNums = history.totalWorkingWeekendNums;

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        newHistory.totalAssignNums[nurse] += totalAssignNums[nurse];
        newHistory.totalWorkingWeekendNums[nurse] +=
            (assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun ));
        newHistory.lastShifts[nurse] = assign[nurse][Weekday::Sun].shift;
        const Consecutive &c( consecutives[nurse] );
        if (assign.isWorking( nurse, Weekday::Sun )) {
            newHistory.consecutiveShiftNums[nurse] =
                c.shiftHigh[Weekday::Sun] - c.shiftLow[Weekday::Sun] + 1;
            newHistory.consecutiveDayNums[nurse] =
                c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
        } else {
            newHistory.consecutiveDayoffNums[nurse] =
                c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
        }
    }

    return newHistory;
}

bool NurseRostering::Solution::repair( const Timer &timer )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
    IterCount startIterCount = iterCount;
#endif
    // must not use swap for swap mode is not compatible with repair mode
    // also, the repair procedure doesn't need the technique to jump through infeasible solutions
    Solution::FindBestMoveTable fbmt = {
        &NurseRostering::Solution::findBestARBoth,
        &NurseRostering::Solution::findBestChange,
        // more move for no tabu condition to get rid of local optima
        &NurseRostering::Solution::findBestAddOnBlockBorder,
        &NurseRostering::Solution::findBestChangeOnBlockBorder
    };

    penalty.setRepairMode();
    ObjValue violation = solver.checkFeasibility( assign );

    bool feasible = (violation == 0);
    if (!feasible) {
        objValue = violation;
        // reduced tabuSearch_Rand()
        {
            int modeNum = fbmt.size();

            const int weight_NoImprove = 256;   // min weight
            const int weight_ImproveCur = 1024; // max weight (less than (RAND_MAX / modeNum))
            const int initWeight = (weight_ImproveCur + weight_NoImprove) / 2;
            const int deltaIncRatio = 8;    // = weights[mode] / weightDelta
            const int incError = deltaIncRatio - 1;
            const int deltaDecRatio = 8;    // = weights[mode] / weightDelta
            const int decError = -(deltaDecRatio - 1);
            vector<int> weights( modeNum, initWeight );
            int totalWeight = initWeight * modeNum;

            for (; !timer.isTimeOut() && (objValue > 0)
                && (iterCount != problem.maxIterCount); ++iterCount) {
                int modeSelect = 0;
                for (int w = solver.randGen() % totalWeight; (w -= weights[modeSelect]) >= 0; ++modeSelect) {}

                Move bestMove;
                (this->*fbmt[modeSelect])(bestMove);

                int weightDelta;
#ifdef INRC2_USE_TABU
                // update tabu list first because it requires original assignment
                ( this->*updateTabuTable[bestMove.mode] )(bestMove);
#endif
                applyBasicMove( bestMove );

                if (bestMove.delta < 0) {    // improve current solution
                    weightDelta = (incError + weight_ImproveCur - weights[modeSelect]) / deltaIncRatio;
                } else {    // no improve but valid
                    weightDelta = (decError + weight_NoImprove - weights[modeSelect]) / deltaDecRatio;
                }

                weights[modeSelect] += weightDelta;
                totalWeight += weightDelta;
            }
        }
        feasible = (objValue == 0);
    }
    penalty.setDefaultMode();

    if (violation != 0) {   // the tabu search has been proceed
        evaluateObjValue();
    }

#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cout << "[RP] iter: " << (iterCount - startIterCount) << ' '
        << "time: " << duration << ' '
        << ((feasible) ? "(success)" : "(fail)") << endl;
#endif
    return feasible;
}

void NurseRostering::Solution::adjustWeightToBiasNurseWithGreaterPenalty( int inverseTotalBiasRatio, int inversePenaltyBiasRatio )
{
    int biasedNurseNum = 0;
    fill( nurseWeights.begin(), nurseWeights.end(), 0 );

    // select worse nurses to meet the PenaltyBiasRatio
    vector<ObjValue> nurseObj( problem.scenario.nurseNum, 0 );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        nurseObj[nurse] += evaluateConsecutiveShift( nurse );
        nurseObj[nurse] += evaluateConsecutiveDay( nurse );
        nurseObj[nurse] += evaluateConsecutiveDayOff( nurse );
        nurseObj[nurse] += evaluatePreference( nurse );
        nurseObj[nurse] += evaluateCompleteWeekend( nurse );
        nurseObj[nurse] += evaluateTotalAssign( nurse );
        nurseObj[nurse] += evaluateTotalWorkingWeekend( nurse );
    }

    class CmpNurseObj
    {
    public:
        CmpNurseObj( const vector<ObjValue> &nurseObjective ) : nurseObj( nurseObjective ) {}
        // sort to (greatest ... least)
        bool operator()( const NurseID &l, const NurseID &r )
        {
            return (nurseObj[l] > nurseObj[r]);
        }

    private:
        const vector<ObjValue> &nurseObj;

    private:    // forbidden operators
        CmpNurseObj& operator=(const CmpNurseObj &) { return *this; }
    };

    CmpNurseObj cmpNurseObj( nurseObj );
    for (auto iter = problem.scenario.contracts.begin();
        iter != problem.scenario.contracts.end(); ++iter) {
        vector<NurseID> &nurses( const_cast<vector<NurseID>&>(iter->nurses) );
        sort( nurses.begin(), nurses.end(), cmpNurseObj );

        int num = nurses.size() / inversePenaltyBiasRatio;
        unsigned remainder = nurses.size() % inversePenaltyBiasRatio;
        biasedNurseNum += num;
        for (int i = 0; i < num; ++i) {
            nurseWeights[nurses[i]] = 1;
        }
        if (solver.randGen() % inversePenaltyBiasRatio < remainder) {
            nurseWeights[nurses[num]] = 1;
            ++biasedNurseNum;
        }
    }

    // pick nurse randomly to meet the TotalBiasRatio
    int num = problem.scenario.nurseNum / inverseTotalBiasRatio;
    while (biasedNurseNum < num) {
        NurseID nurse = solver.randGen() % problem.scenario.nurseNum;
        if (nurseWeights[nurse] == 0) {
            nurseWeights[nurse] = 1;
            ++biasedNurseNum;
        }
    }
}


void NurseRostering::Solution::tabuSearch_Rand( const Timer &timer, const FindBestMoveTable &findBestMoveTable, IterCount maxNoImproveCount )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
    IterCount startIterCount = iterCount;
#endif
    optima = *this;

    int modeNum = findBestMoveTable.size();

    const int weight_Invalid = 128;     // min weight
    const int weight_NoImprove = 256;
    const int weight_ImproveCur = 1024;
    const int weight_ImproveOpt = 4096; // max weight (less than (RAND_MAX / modeNum))
    const int initWeight = (weight_ImproveCur + weight_NoImprove) / 2;
    const int deltaIncRatio = 8;    // = weights[mode] / weightDelta
    const int incError = deltaIncRatio - 1;
    const int deltaDecRatio = 8;    // = weights[mode] / weightDelta
    const int decError = -(deltaDecRatio - 1);
    vector<int> weights( modeNum, initWeight );
    int totalWeight = initWeight * modeNum;

    IterCount noImprove = maxNoImproveCount;
    for (; !timer.isTimeOut() && (noImprove > 0)
        && (iterCount != problem.maxIterCount); ++iterCount) {
        int modeSelect = 0;
        for (int w = solver.randGen() % totalWeight; (w -= weights[modeSelect]) >= 0; ++modeSelect) {}

        Move bestMove;
        (this->*findBestMoveTable[modeSelect])(bestMove);

        int weightDelta;
        if (bestMove.delta < DefaultPenalty::MAX_OBJ_VALUE) {
#ifdef INRC2_USE_TABU
            // update tabu list first because it requires original assignment
            ( this->*updateTabuTable[bestMove.mode] )(bestMove);
#endif
            applyBasicMove( bestMove );

            if (updateOptima()) {   // improve optima
                noImprove = maxNoImproveCount;
                weightDelta = (incError + weight_ImproveOpt - weights[modeSelect]) / deltaIncRatio;
            } else {
                --noImprove;
                if (bestMove.delta < 0) {    // improve current solution
                    weightDelta = (weights[modeSelect] < weight_ImproveCur)
                        ? (incError + weight_ImproveCur - weights[modeSelect]) / deltaIncRatio
                        : (decError + weight_ImproveCur - weights[modeSelect]) / deltaDecRatio;
                } else {    // no improve but valid
                    weightDelta = (weights[modeSelect] < weight_NoImprove)
                        ? (incError + weight_NoImprove - weights[modeSelect]) / deltaIncRatio
                        : (decError + weight_NoImprove - weights[modeSelect]) / deltaDecRatio;
                }
            }
        } else {    // invalid
            weightDelta = (decError + weight_Invalid - weights[modeSelect]) / deltaDecRatio;
        }

        weights[modeSelect] += weightDelta;
        totalWeight += weightDelta;
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cout << "[TS] iter: " << (iterCount - startIterCount) << ' '
        << "time: " << duration << ' '
        << "speed: " << (iterCount - startIterCount) * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}


bool NurseRostering::Solution::findBestAdd( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
#ifdef INRC2_USE_TABU
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;
#endif

    Move move;
    move.mode = Move::Mode::Add;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            if (!assign.isWorking( move.nurse, move.weekday )) {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryAddAssign( move );
#ifdef INRC2_USE_TABU
                        if (noAddTabu( move )) {
#endif
                            if (rs.isMinimal( move.delta, bestMove.delta, solver.randGen )) {
                                bestMove = move;
                            }
#ifdef INRC2_USE_TABU
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta, solver.randGen )) {
                                bestMove_tabu = move;
                            }
                        }
#endif
                    }
                }
            }
        }
    }

#ifdef INRC2_USE_TABU
    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }
#endif

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestChange( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
#ifdef INRC2_USE_TABU
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;
#endif

    Move move;
    move.mode = Move::Mode::Change;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryChangeAssign( move );
#ifdef INRC2_USE_TABU
                        if (noChangeTabu( move )) {
#endif
                            if (rs.isMinimal( move.delta, bestMove.delta, solver.randGen )) {
                                bestMove = move;
                            }
#ifdef INRC2_USE_TABU
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta, solver.randGen )) {
                                bestMove_tabu = move;
                            }
                        }
#endif
                    }
                }
            }
        }
    }

#ifdef INRC2_USE_TABU
    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }
#endif

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestRemove( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
#ifdef INRC2_USE_TABU
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;
#endif

    Move move;
    move.mode = Move::Mode::Remove;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                move.delta = tryRemoveAssign( move );
#ifdef INRC2_USE_TABU
                if (noRemoveTabu( move )) {
#endif
                    if (rs.isMinimal( move.delta, bestMove.delta, solver.randGen )) {
                        bestMove = move;
                    }
#ifdef INRC2_USE_TABU
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta, solver.randGen )) {
                        bestMove_tabu = move;
                    }
                }
#endif
            }
        }
    }

#ifdef INRC2_USE_TABU
    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }
#endif

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestBlockSwap_cached( Move &bestMove ) const
{
    penalty.setBlockSwapMode();

    RandSelect<ObjValue> rs;

    Move move;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.nurse2 = move.nurse + 1; move.nurse2 < problem.scenario.nurseNum; ++move.nurse2) {
            if (((nurseWeights[move.nurse] == 0) && (nurseWeights[move.nurse2] == 0))
                || !solver.haveSameSkill( move.nurse, move.nurse2 )) {
                continue;
            }
            BlockSwapCacheItem &cache( blockSwapCache[move.nurse][move.nurse2] );
            if (!(isBlockSwapCacheValid[move.nurse] && isBlockSwapCacheValid[move.nurse2])) {
                RandSelect<ObjValue> rs_currentPair;
                cache.delta = DefaultPenalty::FORBIDDEN_MOVE;
                for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
                    move.delta = trySwapBlock( move.weekday, move.weekday2, move.nurse, move.nurse2 );
                    if (rs_currentPair.isMinimal( move.delta, cache.delta, solver.randGen )) {
                        cache.delta = move.delta;
                        cache.weekday = move.weekday;
                        cache.weekday2 = move.weekday2;
                    }
                }
            }
            if (rs.isMinimal( cache.delta, bestMove.delta, solver.randGen )) {
                bestMove.mode = Move::Mode::BlockSwap;
                bestMove.delta = cache.delta;
                bestMove.nurse = move.nurse;
                bestMove.nurse2 = move.nurse2;
                bestMove.weekday = cache.weekday;
                bestMove.weekday2 = cache.weekday2;
            }
        }
        isBlockSwapCacheValid[move.nurse] = true;
    }

    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestARBoth( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
#ifdef INRC2_USE_TABU
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;
#endif

    Move move;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                move.delta = tryRemoveAssign( move );
#ifdef INRC2_USE_TABU
                if (noRemoveTabu( move )) {
#endif
                    if (rs.isMinimal( move.delta, bestMove.delta, solver.randGen )) {
                        bestMove = move;
                        bestMove.mode = Move::Mode::Remove;
                    }
#ifdef INRC2_USE_TABU
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta, solver.randGen )) {
                        bestMove_tabu = move;
                        bestMove_tabu.mode = Move::Mode::Remove;
                    }
                }
#endif
            } else {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryAddAssign( move );
#ifdef INRC2_USE_TABU
                        if (noAddTabu( move )) {
#endif
                            if (rs.isMinimal( move.delta, bestMove.delta, solver.randGen )) {
                                bestMove = move;
                                bestMove.mode = Move::Mode::Add;
                            }
#ifdef INRC2_USE_TABU
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta, solver.randGen )) {
                                bestMove_tabu = move;
                                bestMove_tabu.mode = Move::Mode::Add;
                            }
                        }
#endif
                    }
                }
            }
        }
    }

#ifdef INRC2_USE_TABU
    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }
#endif

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestAddOnBlockBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
#ifdef INRC2_USE_TABU
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;
#endif

    Move move;
    move.mode = Move::Mode::Add;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        const Consecutive &c( consecutives[move.nurse] );
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun;) {
            if (!assign.isWorking( move.nurse, move.weekday )) {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryAddAssign( move );
#ifdef INRC2_USE_TABU
                        if (noAddTabu( move )) {
#endif
                            if (rs.isMinimal( move.delta, bestMove.delta, solver.randGen )) {
                                bestMove = move;
                            }
#ifdef INRC2_USE_TABU
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta, solver.randGen )) {
                                bestMove_tabu = move;
                            }
                        }
#endif
                    }
                }
                move.weekday = (move.weekday != c.dayHigh[move.weekday]) ? c.dayHigh[move.weekday] : (move.weekday + 1);
            } else {
                move.weekday = c.dayHigh[move.weekday] + 1;
            }
        }
    }

#ifdef INRC2_USE_TABU
    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }
#endif

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestChangeOnBlockBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
#ifdef INRC2_USE_TABU
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;
#endif

    Move move;
    move.mode = Move::Mode::Change;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        const Consecutive &c( consecutives[move.nurse] );
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun;) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryChangeAssign( move );
#ifdef INRC2_USE_TABU
                        if (noChangeTabu( move )) {
#endif
                            if (rs.isMinimal( move.delta, bestMove.delta, solver.randGen )) {
                                bestMove = move;
                            }
#ifdef INRC2_USE_TABU
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta, solver.randGen )) {
                                bestMove_tabu = move;
                            }
                        }
#endif
                    }
                }
                move.weekday = (move.weekday != c.shiftHigh[move.weekday]) ? c.shiftHigh[move.weekday] : (move.weekday + 1);
            } else {
                move.weekday = c.shiftHigh[move.weekday] + 1;
            }
        }
    }

#ifdef INRC2_USE_TABU
    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }
#endif

    return (bestMove.delta < 0);
}


NurseRostering::ObjValue NurseRostering::Solution::tryAddAssign( int weekday, NurseID nurse, const Assign &a ) const
{
    ObjValue delta = 0;

    // TODO : make sure they won't be the same and leave out this
    if (!a.isWorking() || assign.isWorking( nurse, weekday )) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    // hard constraint check
    delta += penalty.MissSkill() * (!problem.scenario.nurses[nurse].skills[a.skill]);

    delta += penalty.Succession() * (!isValidSuccession( nurse, a.shift, weekday ));
    delta += penalty.Succession() * (!isValidPrior( nurse, a.shift, weekday ));

    if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
        return delta;
    }

    const WeekData &weekData( problem.weekData );
    delta -= penalty.UnderStaff() * (weekData.minNurseNums[weekday][a.shift][a.skill] >
        (weekData.optNurseNums[weekday][a.shift][a.skill] - missingNurseNums[weekday][a.shift][a.skill]));

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    ContractID contractID = problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta -= penalty.InsufficientStaff() *
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
            delta -= penalty.ConsecutiveShift() *
                distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sat],
                prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
            delta += penalty.ConsecutiveShift() * exceedCount(
                Weekday::Sun - c.shiftLow[Weekday::Sat] + 1, shift.maxConsecutiveShiftNum );
        } else {    // have nothing to do with previous block
            delta += penalty.ConsecutiveShift() *    // penalty on day off is counted later
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
                delta -= penalty.ConsecutiveShift() *
                    distanceToRange( weekday - c.shiftLow[prevDay],
                    prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
            }
            if (nextShiftID == a.shift) {
                const Scenario::Shift &nextShift( shifts[nextShiftID] );
                high = c.shiftHigh[nextDay];
                delta -= penalty.ConsecutiveShift() * penaltyDayNum(
                    c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                    nextShift.minConsecutiveShiftNum, nextShift.maxConsecutiveShiftNum );
            }
            delta += penalty.ConsecutiveShift() * penaltyDayNum( high - low + 1, high,
                shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
        } else if (weekday == c.shiftHigh[weekday]) {
            if (a.shift == nextShiftID) {
                const Scenario::Shift &nextShift( shifts[nextShiftID] );
                int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                if (consecutiveShiftOfNextBlock >= nextShift.maxConsecutiveShiftNum) {
                    delta += penalty.ConsecutiveShift();
                } else if ((c.shiftHigh[nextDay] < Weekday::Sun)
                    && (consecutiveShiftOfNextBlock < nextShift.minConsecutiveShiftNum)) {
                    delta -= penalty.ConsecutiveShift();
                }
            } else {
                delta += penalty.ConsecutiveShift() * distanceToRange( 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            }
        } else if (weekday == c.shiftLow[weekday]) {
            if (a.shift == prevShiftID) {
                const Scenario::Shift &prevShift( shifts[prevShiftID] );
                int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                if (consecutiveShiftOfPrevBlock >= prevShift.maxConsecutiveShiftNum) {
                    delta += penalty.ConsecutiveShift();
                } else if (consecutiveShiftOfPrevBlock < prevShift.minConsecutiveShiftNum) {
                    delta -= penalty.ConsecutiveShift();
                }
            } else {
                delta += penalty.ConsecutiveShift() * distanceToRange( 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            }
        } else {
            delta += penalty.ConsecutiveShift() * distanceToRange( 1,
                shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
        }
    }

    // consecutive day and day-off
    if (weekday == Weekday::Sun) {  // there is no block on the right
        // dayHigh[weekday] will always be equal to Weekday::Sun
        if (Weekday::Sun == c.dayLow[weekday]) {
            delta -= penalty.ConsecutiveDay() *
                distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= penalty.ConsecutiveDayOff() *
                exceedCount( 1, contract.maxConsecutiveDayoffNum );
            delta += penalty.ConsecutiveDay() * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sat] + 1, contract.maxConsecutiveDayNum );
        } else {    // day off block length over 1
            delta -= penalty.ConsecutiveDayOff() * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sun] + 1, contract.maxConsecutiveDayoffNum );
            delta += penalty.ConsecutiveDayOff() * distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sun],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += penalty.ConsecutiveDay() * exceedCount( 1, contract.maxConsecutiveDayNum );
        }
    } else {
        if (c.dayHigh[weekday] == c.dayLow[weekday]) {
            delta -= penalty.ConsecutiveDay() * distanceToRange( weekday - c.dayLow[prevDay],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= penalty.ConsecutiveDayOff() * distanceToRange( 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= penalty.ConsecutiveDay() * penaltyDayNum(
                c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += penalty.ConsecutiveDay() * penaltyDayNum(
                c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        } else if (weekday == c.dayHigh[weekday]) {
            int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
            if (consecutiveDayOfNextBlock >= contract.maxConsecutiveDayNum) {
                delta += penalty.ConsecutiveDay();
            } else if ((c.dayHigh[nextDay] < Weekday::Sun)
                && (consecutiveDayOfNextBlock < contract.minConsecutiveDayNum)) {
                delta -= penalty.ConsecutiveDay();
            }
            int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= penalty.ConsecutiveDayOff();
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += penalty.ConsecutiveDayOff();
            }
        } else if (weekday == c.dayLow[weekday]) {
            int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
            if (consecutiveDayOfPrevBlock >= contract.maxConsecutiveDayNum) {
                delta += penalty.ConsecutiveDay();
            } else if (consecutiveDayOfPrevBlock < contract.minConsecutiveDayNum) {
                delta -= penalty.ConsecutiveDay();
            }
            int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= penalty.ConsecutiveDayOff();
            } else if ((c.dayHigh[weekday] < Weekday::Sun)
                && (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum)) {
                delta += penalty.ConsecutiveDayOff();
            }
        } else {
            delta -= penalty.ConsecutiveDayOff() * penaltyDayNum(
                c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += penalty.ConsecutiveDayOff() *
                distanceToRange( weekday - c.dayLow[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += penalty.ConsecutiveDay() * distanceToRange( 1,
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += penalty.ConsecutiveDayOff() * penaltyDayNum(
                c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
        }
    }

    // preference
    delta += penalty.Preference() *
        weekData.shiftOffs[weekday][a.shift][nurse];

    if (weekday > Weekday::Fri) {
        int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
        // complete weekend
        if (contract.completeWeekend) {
            if (assign.isWorking( nurse, theOtherDay )) {
                delta -= penalty.CompleteWeekend();
            } else {
                delta += penalty.CompleteWeekend();
            }
        }

        // total working weekend
        if (!assign.isWorking( nurse, theOtherDay )) {
#ifdef INRC2_AVERAGE_MAX_WORKING_WEEKEND
            const History &history( problem.history );
            int currentWeek = history.currentWeek;
            delta -= penalty.TotalWorkingWeekend() * exceedCount(
                history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
            delta += penalty.TotalWorkingWeekend() * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
#else
            delta -= penalty.TotalWorkingWeekend() * exceedCount( 0,
                problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
            delta += penalty.TotalWorkingWeekend() * exceedCount( problem.history.restWeekCount,
                problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
#endif
        }
    }

    // total assign (expand problem.history.restWeekCount times)
    int restMinShift = problem.scenario.nurses[nurse].restMinShiftNum;
    int restMaxShift = problem.scenario.nurses[nurse].restMaxShiftNum;
    int totalAssign = totalAssignNums[nurse] * problem.history.restWeekCount;
    delta -= penalty.TotalAssign() * distanceToRange( totalAssign,
        restMinShift, restMaxShift ) / problem.history.restWeekCount;
    delta += penalty.TotalAssign() * distanceToRange( totalAssign + problem.history.restWeekCount,
        restMinShift, restMaxShift ) / problem.history.restWeekCount;

    return delta;   // TODO : weight ?
}

NurseRostering::ObjValue NurseRostering::Solution::tryAddAssign( const Move &move ) const
{
    return tryAddAssign( move.weekday, move.nurse, move.assign );
}

NurseRostering::ObjValue NurseRostering::Solution::tryChangeAssign( int weekday, NurseID nurse, const Assign &a ) const
{
    ObjValue delta = 0;

    ShiftID oldShiftID = assign[nurse][weekday].shift;
    SkillID oldSkillID = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if (!a.isWorking() || !Assign::isWorking( oldShiftID )
        || ((a.shift == oldShiftID) && (a.skill == oldSkillID))) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    delta += penalty.MissSkill() * (!problem.scenario.nurses[nurse].skills[a.skill]);

    delta += penalty.Succession() * (!isValidSuccession( nurse, a.shift, weekday ));
    delta += penalty.Succession() * (!isValidPrior( nurse, a.shift, weekday ));

    const WeekData &weekData( problem.weekData );
    delta += penalty.UnderStaff() * (weekData.minNurseNums[weekday][oldShiftID][oldSkillID] >=
        (weekData.optNurseNums[weekday][oldShiftID][oldSkillID] - missingNurseNums[weekday][oldShiftID][oldSkillID]));

    if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
        return delta;
    }

    delta -= penalty.Succession() * (!isValidSuccession( nurse, oldShiftID, weekday ));
    delta -= penalty.Succession() * (!isValidPrior( nurse, oldShiftID, weekday ));

    delta -= penalty.UnderStaff() * (weekData.minNurseNums[weekday][a.shift][a.skill] >
        (weekData.optNurseNums[weekday][a.shift][a.skill] - missingNurseNums[weekday][a.shift][a.skill]));

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta += penalty.InsufficientStaff() *
        (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);
    delta -= penalty.InsufficientStaff() *
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
                    delta -= penalty.ConsecutiveShift() *
                        distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sat],
                        prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                    delta -= penalty.ConsecutiveShift() *
                        exceedCount( 1, oldShift.maxConsecutiveShiftNum );
                    delta += penalty.ConsecutiveShift() * exceedCount(
                        Weekday::Sun - c.shiftLow[Weekday::Sat] + 1, shift.maxConsecutiveShiftNum );
                } else {
                    delta -= penalty.ConsecutiveShift() *
                        exceedCount( 1, oldShift.maxConsecutiveShiftNum );
                    delta += penalty.ConsecutiveShift() *
                        exceedCount( 1, shift.maxConsecutiveShiftNum );
                }
            } else {    // block length over 1
                delta -= penalty.ConsecutiveShift() * exceedCount(
                    Weekday::Sun - c.shiftLow[Weekday::Sun] + 1, oldShift.maxConsecutiveShiftNum );
                delta += penalty.ConsecutiveShift() * distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sun],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += penalty.ConsecutiveShift() *
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
                    delta -= penalty.ConsecutiveShift() *
                        distanceToRange( weekday - c.shiftLow[prevDay],
                        prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                }
                if (nextShiftID == a.shift) {
                    const Scenario::Shift &nextShift( shifts[nextShiftID] );
                    high = c.shiftHigh[nextDay];
                    delta -= penalty.ConsecutiveShift() * penaltyDayNum(
                        c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                        nextShift.minConsecutiveShiftNum, nextShift.maxConsecutiveShiftNum );
                }
                delta -= penalty.ConsecutiveShift() * distanceToRange( 1,
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += penalty.ConsecutiveShift() * penaltyDayNum( high - low + 1, high,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            } else if (weekday == c.shiftHigh[weekday]) {
                if (nextShiftID == a.shift) {
                    const Scenario::Shift &nextShift( shifts[nextShiftID] );
                    int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                    if (consecutiveShiftOfNextBlock >= nextShift.maxConsecutiveShiftNum) {
                        delta += penalty.ConsecutiveShift();
                    } else if ((c.shiftHigh[nextDay] < Weekday::Sun)
                        && (consecutiveShiftOfNextBlock < nextShift.minConsecutiveShiftNum)) {
                        delta -= penalty.ConsecutiveShift();
                    }
                } else {
                    delta += penalty.ConsecutiveShift() * distanceToRange( 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
                int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
                if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                    delta -= penalty.ConsecutiveShift();
                } else if (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum) {
                    delta += penalty.ConsecutiveShift();
                }
            } else if (weekday == c.shiftLow[weekday]) {
                if (prevShiftID == a.shift) {
                    const Scenario::Shift &prevShift( shifts[prevShiftID] );
                    int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                    if (consecutiveShiftOfPrevBlock >= prevShift.maxConsecutiveShiftNum) {
                        delta += penalty.ConsecutiveShift();
                    } else if (consecutiveShiftOfPrevBlock < prevShift.minConsecutiveShiftNum) {
                        delta -= penalty.ConsecutiveShift();
                    }
                } else {
                    delta += penalty.ConsecutiveShift() * distanceToRange( 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
                int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
                if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                    delta -= penalty.ConsecutiveShift();
                } else if ((c.shiftHigh[weekday] < Weekday::Sun)
                    && (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum)) {
                    delta += penalty.ConsecutiveShift();
                }
            } else {
                delta -= penalty.ConsecutiveShift() * penaltyDayNum(
                    c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += penalty.ConsecutiveShift() *
                    distanceToRange( weekday - c.shiftLow[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += penalty.ConsecutiveShift() * distanceToRange( 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                delta += penalty.ConsecutiveShift() *
                    penaltyDayNum( c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            }
        }

        // preference
        delta += penalty.Preference() *
            weekData.shiftOffs[weekday][a.shift][nurse];
        delta -= penalty.Preference() *
            weekData.shiftOffs[weekday][oldShiftID][nurse];
    }

    return delta;   // TODO : weight ?
}

NurseRostering::ObjValue NurseRostering::Solution::tryChangeAssign( const Move &move ) const
{
    return tryChangeAssign( move.weekday, move.nurse, move.assign );
}

NurseRostering::ObjValue NurseRostering::Solution::tryRemoveAssign( int weekday, NurseID nurse ) const
{
    ObjValue delta = 0;

    ShiftID oldShiftID = assign[nurse][weekday].shift;
    SkillID oldSkillID = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if (!Assign::isWorking( oldShiftID )) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    const WeekData &weekData( problem.weekData );
    delta += penalty.UnderStaff() * (weekData.minNurseNums[weekday][oldShiftID][oldSkillID] >=
        (weekData.optNurseNums[weekday][oldShiftID][oldSkillID] - missingNurseNums[weekday][oldShiftID][oldSkillID]));

    if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
        return delta;
    }

    delta -= penalty.Succession() * (!isValidSuccession( nurse, oldShiftID, weekday ));
    delta -= penalty.Succession() * (!isValidPrior( nurse, oldShiftID, weekday ));

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    ContractID contractID = problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta += penalty.InsufficientStaff() *
        (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);

    if (nurseWeights[nurse] == 0) {
        return delta;   // TODO : weight ?
    }

    // consecutive shift
    const vector<Scenario::Shift> &shifts( problem.scenario.shifts );
    const Scenario::Shift &oldShift( shifts[oldShiftID] );
    if (weekday == Weekday::Sun) {  // there is no block on the right
        if (Weekday::Sun == c.shiftLow[weekday]) {
            delta -= penalty.ConsecutiveShift() * exceedCount( 1, oldShift.maxConsecutiveShiftNum );
        } else {
            delta -= penalty.ConsecutiveShift() * exceedCount(
                Weekday::Sun - c.shiftLow[weekday] + 1, oldShift.maxConsecutiveShiftNum );
            delta += penalty.ConsecutiveShift() * distanceToRange( Weekday::Sun - c.shiftLow[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
        }
    } else {
        if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
            delta -= penalty.ConsecutiveShift() * distanceToRange( 1,
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
        } else if (weekday == c.shiftHigh[weekday]) {
            int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
            if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                delta -= penalty.ConsecutiveShift();
            } else if (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum) {
                delta += penalty.ConsecutiveShift();
            }
        } else if (weekday == c.shiftLow[weekday]) {
            int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
            if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                delta -= penalty.ConsecutiveShift();
            } else if ((c.shiftHigh[weekday] < Weekday::Sun)
                && (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum)) {
                delta += penalty.ConsecutiveShift();
            }
        } else {
            delta -= penalty.ConsecutiveShift() * penaltyDayNum(
                c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            delta += penalty.ConsecutiveShift() * distanceToRange( weekday - c.shiftLow[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            delta += penalty.ConsecutiveShift() * penaltyDayNum(
                c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
        }
    }

    // consecutive day and day-off
    if (weekday == Weekday::Sun) {  // there is no blocks on the right
        // dayHigh[weekday] will always be equal to Weekday::Sun
        if (Weekday::Sun == c.dayLow[weekday]) {
            delta -= penalty.ConsecutiveDayOff() *
                distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= penalty.ConsecutiveDay() *
                exceedCount( 1, contract.maxConsecutiveDayNum );
            delta += penalty.ConsecutiveDayOff() * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sat] + 1, contract.maxConsecutiveDayoffNum );
        } else {    // day off block length over 1
            delta -= penalty.ConsecutiveDay() * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sun] + 1, contract.maxConsecutiveDayNum );
            delta += penalty.ConsecutiveDay() * distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sun],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += penalty.ConsecutiveDayOff() * exceedCount( 1, contract.maxConsecutiveDayoffNum );
        }
    } else {
        if (c.dayHigh[weekday] == c.dayLow[weekday]) {
            delta -= penalty.ConsecutiveDayOff() *
                distanceToRange( weekday - c.dayLow[prevDay],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= penalty.ConsecutiveDay() *
                distanceToRange( 1,
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= penalty.ConsecutiveDayOff() * penaltyDayNum(
                c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += penalty.ConsecutiveDayOff() * penaltyDayNum(
                c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
        } else if (weekday == c.dayHigh[weekday]) {
            int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
            if (consecutiveDayOfNextBlock >= contract.maxConsecutiveDayoffNum) {
                delta += penalty.ConsecutiveDayOff();
            } else if ((c.dayHigh[nextDay] < Weekday::Sun)
                && (consecutiveDayOfNextBlock < contract.minConsecutiveDayoffNum)) {
                delta -= penalty.ConsecutiveDayOff();
            }
            int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayNum) {
                delta -= penalty.ConsecutiveDay();
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayNum) {
                delta += penalty.ConsecutiveDay();
            }
        } else if (weekday == c.dayLow[weekday]) {
            int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
            if (consecutiveDayOfPrevBlock >= contract.maxConsecutiveDayoffNum) {
                delta += penalty.ConsecutiveDayOff();
            } else if (consecutiveDayOfPrevBlock < contract.minConsecutiveDayoffNum) {
                delta -= penalty.ConsecutiveDayOff();
            }
            int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayNum) {
                delta -= penalty.ConsecutiveDay();
            } else if ((c.dayHigh[weekday] < Weekday::Sun)
                && (consecutiveDayOfThisBlock <= contract.minConsecutiveDayNum)) {
                delta += penalty.ConsecutiveDay();
            }
        } else {
            delta -= penalty.ConsecutiveDay() * penaltyDayNum(
                c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += penalty.ConsecutiveDay() *
                distanceToRange( weekday - c.dayLow[weekday],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += penalty.ConsecutiveDayOff() * distanceToRange( 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += penalty.ConsecutiveDay() *
                penaltyDayNum( c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        }
    }

    // preference
    delta -= penalty.Preference() *
        weekData.shiftOffs[weekday][oldShiftID][nurse];

    if (weekday > Weekday::Fri) {
        int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
        // complete weekend
        if (contract.completeWeekend) {
            if (assign.isWorking( nurse, theOtherDay )) {
                delta += penalty.CompleteWeekend();
            } else {
                delta -= penalty.CompleteWeekend();
            }
        }

        // total working weekend
        if (!assign.isWorking( nurse, theOtherDay )) {
#ifdef INRC2_AVERAGE_MAX_WORKING_WEEKEND
            const History &history( problem.history );
            int currentWeek = history.currentWeek;
            delta -= penalty.TotalWorkingWeekend() * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
            delta += penalty.TotalWorkingWeekend() * exceedCount(
                history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
#else
            delta -= penalty.TotalWorkingWeekend() * exceedCount( problem.history.restWeekCount,
                problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
            delta += penalty.TotalWorkingWeekend() * exceedCount( 0,
                problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
#endif
        }
    }

    // total assign (expand problem.history.restWeekCount times)
    int restMinShift = problem.scenario.nurses[nurse].restMinShiftNum;
    int restMaxShift = problem.scenario.nurses[nurse].restMaxShiftNum;
    int totalAssign = totalAssignNums[nurse] * problem.history.restWeekCount;
    delta -= penalty.TotalAssign() * distanceToRange( totalAssign,
        restMinShift, restMaxShift ) / problem.history.restWeekCount;
    delta += penalty.TotalAssign() * distanceToRange( totalAssign - problem.history.restWeekCount,
        restMinShift, restMaxShift ) / problem.history.restWeekCount;

    return delta;   // TODO : weight ?
}

NurseRostering::ObjValue NurseRostering::Solution::tryRemoveAssign( const Move &move ) const
{
    return tryRemoveAssign( move.weekday, move.nurse );
}

NurseRostering::ObjValue NurseRostering::Solution::trySwapNurse( int weekday, NurseID nurse, NurseID nurse2 ) const
{
    // TODO : make sure they won't be the same and leave out this
    if (nurse == nurse2) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    if (assign.isWorking( nurse, weekday )) {
        if (assign.isWorking( nurse2, weekday )) {
            nurseDelta = tryChangeAssign( weekday, nurse, assign[nurse2][weekday] );
            nurse2Delta = ((nurseDelta >= DefaultPenalty::MAX_OBJ_VALUE)
                ? 0 : tryChangeAssign( weekday, nurse2, assign[nurse][weekday] ));
        } else {
            nurseDelta = tryRemoveAssign( weekday, nurse );
            nurse2Delta = ((nurseDelta >= DefaultPenalty::MAX_OBJ_VALUE)
                ? 0 : tryAddAssign( weekday, nurse2, assign[nurse][weekday] ));
        }
    } else {
        if (assign.isWorking( nurse2, weekday )) {
            nurseDelta = tryAddAssign( weekday, nurse, assign[nurse2][weekday] );
            nurse2Delta = ((nurseDelta >= DefaultPenalty::MAX_OBJ_VALUE)
                ? 0 : tryRemoveAssign( weekday, nurse2 ));
        } else {    // no change
            nurseDelta = 0;
            nurse2Delta = 0;
            return DefaultPenalty::FORBIDDEN_MOVE;
        }
    }

    return (nurseDelta + nurse2Delta);
}

NurseRostering::ObjValue NurseRostering::Solution::trySwapBlock( int weekday, int &weekday2, NurseID nurse, NurseID nurse2 ) const
{
    // TODO : make sure they won't be the same and leave out this
    if (nurse == nurse2) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    if (!(isValidSuccession( nurse, assign[nurse2][weekday].shift, weekday )
        && isValidSuccession( nurse2, assign[nurse][weekday].shift, weekday ))) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    RandSelect<ObjValue> rs;
    ObjValue delta = 0;
    ObjValue delta1 = 0;
    ObjValue delta2 = 0;
    ObjValue minDelta = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurseDelta = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurse2Delta = DefaultPenalty::FORBIDDEN_MOVE;
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
    RandSelect<ObjValue> rs_tabu;
    ObjValue minDelta_tabu = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurseDelta_tabu = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurse2Delta_tabu = DefaultPenalty::FORBIDDEN_MOVE;
    int weekday2_tabu = weekday;

    int count = 0;
    int noTabuCount = 0;
#endif
    // try each block length
    int w = weekday;
    while (problem.scenario.nurses[nurse].skills[assign[nurse2][w].skill]
        && problem.scenario.nurses[nurse2].skills[assign[nurse][w].skill]) {
        // longer blocks will also miss this skill
        delta += trySwapNurse( w, nurse, nurse2 );
        delta1 += nurseDelta;
        delta2 += nurse2Delta;

#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
        ++count;
        noTabuCount += noSwapTabu( w, nurse, nurse2 );
#endif

        if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
            if (isValidPrior( nurse, assign[nurse2][w].shift, w )
                && isValidPrior( nurse2, assign[nurse][w].shift, w )) {
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
                if (noBlockSwapTabu( noTabuCount, count )) {
#endif
                    if (rs.isMinimal( delta, minDelta, solver.randGen )) {
                        minDelta = delta;
                        weekday2 = w;
                        minNurseDelta = delta1;
                        minNurse2Delta = delta2;
                    }
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
                } else {    // tabu
                    if (rs_tabu.isMinimal( delta, minDelta_tabu, solver.randGen )) {
                        minDelta_tabu = delta;
                        weekday2_tabu = w;
                        minNurseDelta_tabu = delta1;
                        minNurse2Delta_tabu = delta2;
                    }
                }
#endif
            }
        } else {    // two day off
            delta -= DefaultPenalty::FORBIDDEN_MOVE;
        }

        if (w >= Weekday::Sun) { break; }

        (const_cast<Solution*>(this))->swapNurse( w, nurse, nurse2 );
        ++w;
    }

    nurseDelta = minNurseDelta;
    nurse2Delta = minNurse2Delta;
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
    if (aspirationCritiera( minDelta, minDelta_tabu )) {
        minDelta = minDelta_tabu;
        weekday2 = weekday2_tabu;
        nurseDelta = minNurseDelta_tabu;
        nurse2Delta = minNurse2Delta_tabu;
    }
#endif

    // recover original data
    while ((--w) >= weekday) {
        (const_cast<Solution*>(this))->swapNurse( w, nurse, nurse2 );
    }

    return minDelta;
}

NurseRostering::ObjValue NurseRostering::Solution::trySwapBlock( const Move &move ) const
{
    penalty.setBlockSwapMode();
    ObjValue delta = trySwapBlock( move.weekday, move.weekday2, move.nurse, move.nurse2 );
    penalty.setDefaultMode();

    return delta;
}


void NurseRostering::Solution::addAssign( int weekday, NurseID nurse, const Assign &a )
{
    updateConsecutive( weekday, nurse, a.shift );

    --missingNurseNums[weekday][a.shift][a.skill];

    ++totalAssignNums[nurse];

    assign[nurse][weekday] = a;
}

void NurseRostering::Solution::addAssign( const Move &move )
{
    addAssign( move.weekday, move.nurse, move.assign );
}

void NurseRostering::Solution::changeAssign( int weekday, NurseID nurse, const Assign &a )
{
    if (a.shift != assign[nurse][weekday].shift) {  // for just change skill
        updateConsecutive( weekday, nurse, a.shift );
    }

    --missingNurseNums[weekday][a.shift][a.skill];
    ++missingNurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];

    assign[nurse][weekday] = a;
}

void NurseRostering::Solution::changeAssign( const Move &move )
{
    changeAssign( move.weekday, move.nurse, move.assign );
}

void NurseRostering::Solution::removeAssign( int weekday, NurseID nurse )
{
    updateConsecutive( weekday, nurse, NurseRostering::Scenario::Shift::ID_NONE );

    ++missingNurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];

    --totalAssignNums[nurse];

    assign[nurse][weekday] = Assign();
}

void NurseRostering::Solution::removeAssign( const Move &move )
{
    removeAssign( move.weekday, move.nurse );
}

void NurseRostering::Solution::swapNurse( int weekday, NurseID nurse, NurseID nurse2 )
{
    if (assign.isWorking( nurse, weekday )) {
        if (assign.isWorking( nurse2, weekday )) {
            Assign a( assign[nurse][weekday] );
            changeAssign( weekday, nurse, assign[nurse2][weekday] );
            changeAssign( weekday, nurse2, a );
        } else {
            addAssign( weekday, nurse2, assign[nurse][weekday] );
            removeAssign( weekday, nurse );
        }
    } else {
        if (assign.isWorking( nurse2, weekday )) {
            addAssign( weekday, nurse, assign[nurse2][weekday] );
            removeAssign( weekday, nurse2 );
        }
    }
}

void NurseRostering::Solution::swapNurse( const Move &move )
{
    swapNurse( move.weekday, move.nurse, move.nurse2 );
}

void NurseRostering::Solution::swapBlock( int weekday, int weekday2, NurseID nurse, NurseID nurse2 )
{
    for (; weekday <= weekday2; ++weekday) {
        swapNurse( weekday, nurse, nurse2 );
    }
}

void NurseRostering::Solution::swapBlock( const Move &move )
{
    swapBlock( move.weekday, move.weekday2, move.nurse, move.nurse2 );
}

void NurseRostering::Solution::updateConsecutive( int weekday, NurseID nurse, ShiftID shift )
{
    Consecutive &c( consecutives[nurse] );
    int nextDay = weekday + 1;
    int prevDay = weekday - 1;

    // consider day
    bool isDayHigh = (weekday == c.dayHigh[weekday]);
    bool isDayLow = (weekday == c.dayLow[weekday]);
    if (assign.isWorking( nurse, weekday ) != Assign::isWorking( shift )) {
        if (isDayHigh && isDayLow) {
            assignSingle( weekday, c.dayHigh, c.dayLow, (weekday != Weekday::Sun), true );
        } else if (isDayHigh) {
            assignHigh( weekday, c.dayHigh, c.dayLow, (weekday != Weekday::Sun) );
        } else if (isDayLow) {
            assignLow( weekday, c.dayHigh, c.dayLow, true );
        } else {
            assignMiddle( weekday, c.dayHigh, c.dayLow );
        }
    }

    // consider shift
    bool isShiftHigh = (weekday == c.shiftHigh[weekday]);
    bool isShiftLow = (weekday == c.shiftLow[weekday]);
    if (isShiftHigh && isShiftLow) {
        assignSingle( weekday, c.shiftHigh, c.shiftLow,
            ((nextDay <= Weekday::Sun) && (shift == assign[nurse][nextDay].shift)),
            (shift == assign[nurse][prevDay].shift) );
    } else if (isShiftHigh) {
        assignHigh( weekday, c.shiftHigh, c.shiftLow,
            ((nextDay <= Weekday::Sun) && (shift == assign[nurse][nextDay].shift)) );
    } else if (isShiftLow) {
        assignLow( weekday, c.shiftHigh, c.shiftLow,
            (shift == assign[nurse][prevDay].shift) );
    } else {
        assignMiddle( weekday, c.shiftHigh, c.shiftLow );
    }
}

void NurseRostering::Solution::assignHigh( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight )
{
    int nextDay = weekday + 1;
    int prevDay = weekday - 1;
    for (int d = prevDay; (d >= Weekday::HIS) && (d >= low[weekday]); --d) {
        high[d] = prevDay;
    }
    if (affectRight) {
        for (int d = nextDay; d <= high[nextDay]; ++d) {
            low[d] = weekday;
        }
        high[weekday] = high[nextDay];
    } else {
        high[weekday] = weekday;
    }
    low[weekday] = weekday;
}

void NurseRostering::Solution::assignLow( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectLeft )
{
    int nextDay = weekday + 1;
    int prevDay = weekday - 1;
    for (int d = nextDay; d <= high[weekday]; ++d) {
        low[d] = nextDay;
    }
    if (affectLeft) {
        for (int d = prevDay; (d >= Weekday::HIS) && (d >= low[prevDay]); --d) {
            high[d] = weekday;
        }
        low[weekday] = low[prevDay];
    } else {
        low[weekday] = weekday;
    }
    high[weekday] = weekday;
}

void NurseRostering::Solution::assignMiddle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE] )
{
    int nextDay = weekday + 1;
    int prevDay = weekday - 1;
    for (int d = nextDay; d <= high[weekday]; ++d) {
        low[d] = nextDay;
    }
    for (int d = prevDay; (d >= Weekday::HIS) && (d >= low[weekday]); --d) {
        high[d] = prevDay;
    }
    high[weekday] = weekday;
    low[weekday] = weekday;
}

void NurseRostering::Solution::assignSingle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight, bool affectLeft )
{
    int nextDay = weekday + 1;
    int prevDay = weekday - 1;
    int h = affectRight ? high[nextDay] : weekday;
    int l = affectLeft ? low[prevDay] : weekday;
    if (affectRight) {
        for (int d = nextDay; d <= high[nextDay]; ++d) {
            low[d] = l;
        }
        high[weekday] = h;
    }
    if (affectLeft) {
        for (int d = prevDay; (d >= Weekday::HIS) && (d >= low[prevDay]); --d) {
            high[d] = h;
        }
        low[weekday] = l;
    }
}


#ifdef INRC2_USE_TABU
void NurseRostering::Solution::updateDayTabu( NurseID nurse, int weekday )
{
    dayTabu[nurse][weekday] = iterCount +
        solver.DayTabuTenureBase() + (solver.randGen() % solver.DayTabuTenureAmp());
}

void NurseRostering::Solution::updateShiftTabu( NurseID nurse, int weekday, const Assign &a )
{
    shiftTabu[nurse][weekday][a.shift][a.skill] = iterCount +
        solver.ShiftTabuTenureBase() + (solver.randGen() % solver.ShiftTabuTenureAmp());
}
#endif


NurseRostering::ObjValue NurseRostering::Solution::evaluateInsufficientStaff()
{
    ObjValue obj = 0;

    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        for (ShiftID shift = NurseRostering::Scenario::Shift::ID_BEGIN;
            shift < problem.scenario.shiftSize; ++shift) {
            for (SkillID skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                skill < problem.scenario.skillSize; ++skill) {
                if (missingNurseNums[weekday][shift][skill] > 0) {
                    obj += penalty.InsufficientStaff() * missingNurseNums[weekday][shift][skill];
                }
            }
        }
    }

    return obj;
}

NurseRostering::ObjValue NurseRostering::Solution::evaluateConsecutiveShift( NurseID nurse )
{
    ObjValue obj = 0;

    const History &history( problem.history );
    const Consecutive &c( consecutives[nurse] );
    const vector<Scenario::Shift> &shifts( problem.scenario.shifts );

    int nextday = c.shiftHigh[Weekday::Mon] + 1;
    if (nextday <= Weekday::Sun) {   // the entire week is not one block
        // handle first block with history
        if (assign.isWorking( nurse, Weekday::Mon )) {
            const Scenario::Shift &shift( shifts[assign[nurse][Weekday::Mon].shift] );
            if (history.lastShifts[nurse] == assign[nurse][Weekday::Mon].shift) {
                if (history.consecutiveShiftNums[nurse] > shift.maxConsecutiveShiftNum) {
                    // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                    obj += penalty.ConsecutiveShift() * (c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    obj += penalty.ConsecutiveShift() * distanceToRange(
                        c.shiftHigh[Weekday::Mon] - c.shiftLow[Weekday::Mon] + 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
            } else {
                obj += penalty.ConsecutiveShift() * distanceToRange(
                    c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1, shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                if (Assign::isWorking( history.lastShifts[nurse] )) {
                    obj += penalty.ConsecutiveShift() * absentCount(
                        history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                }
            }
        } else if (Assign::isWorking( history.lastShifts[nurse] )) {
            obj += penalty.ConsecutiveShift() * absentCount(
                history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
        }
        // handle blocks in the middle of the week
        for (; c.shiftHigh[nextday] < Weekday::Sun; nextday = c.shiftHigh[nextday] + 1) {
            if (assign.isWorking( nurse, nextday )) {
                const ShiftID &shiftID( assign[nurse][nextday].shift );
                const Scenario::Shift &shift( shifts[shiftID] );
                obj += penalty.ConsecutiveShift() *
                    distanceToRange( c.shiftHigh[nextday] - c.shiftLow[nextday] + 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            }
        }
    }
    // handle last consecutive block
    int consecutiveShift_EntireWeek = history.consecutiveShiftNums[nurse] + Weekday::NUM;
    int consecutiveShift = c.shiftHigh[Weekday::Sun] - c.shiftLow[Weekday::Sun] + 1;
    if (assign.isWorking( nurse, Weekday::Sun )) {
        const ShiftID &shiftID( assign[nurse][Weekday::Sun].shift );
        const Scenario::Shift &shift( problem.scenario.shifts[shiftID] );
        if (c.isSingleConsecutiveShift()) { // the entire week is one block
            if (history.lastShifts[nurse] == assign[nurse][Weekday::Sun].shift) {
                if (history.consecutiveShiftNums[nurse] > shift.maxConsecutiveShiftNum) {
                    obj += penalty.ConsecutiveShift() * Weekday::NUM;
                } else {
                    obj += penalty.ConsecutiveShift() * exceedCount(
                        consecutiveShift_EntireWeek, shift.maxConsecutiveShiftNum );
                }
            } else {    // different shifts
                if (Weekday::NUM > shift.maxConsecutiveShiftNum) {
                    obj += penalty.ConsecutiveShift() *
                        (Weekday::NUM - shift.maxConsecutiveShiftNum);
                }
                if (Assign::isWorking( history.lastShifts[nurse] )) {
                    obj += penalty.ConsecutiveShift() * absentCount(
                        history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                }
            }
        } else {
            obj += penalty.ConsecutiveShift() * exceedCount(
                consecutiveShift, shift.maxConsecutiveShiftNum );
        }
    } else if (c.isSingleConsecutiveShift() // the entire week is one block
        && Assign::isWorking( history.lastShifts[nurse] )) {
        obj += penalty.ConsecutiveShift() * absentCount(
            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
    }

    return obj;
}

NurseRostering::ObjValue NurseRostering::Solution::evaluateConsecutiveDay( NurseID nurse )
{
    ObjValue obj = 0;

    const History &history( problem.history );
    const Consecutive &c( consecutives[nurse] );
    const ContractID &contractID( problem.scenario.nurses[nurse].contract );
    const Scenario::Contract &contract( problem.scenario.contracts[contractID] );

    int nextday = c.dayHigh[Weekday::Mon] + 1;
    if (nextday <= Weekday::Sun) {   // the entire week is not one block
        // handle first block with history
        if (assign.isWorking( nurse, Weekday::Mon )) {
            if (history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                obj += penalty.ConsecutiveDay() * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
            } else {
                obj += penalty.ConsecutiveDay() * distanceToRange(
                    c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            }
        } else if (Assign::isWorking( history.lastShifts[nurse] )) {
            obj += penalty.ConsecutiveDay() * absentCount
                ( history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
        }
        // handle blocks in the middle of the week
        for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
            if (assign.isWorking( nurse, nextday )) {
                obj += penalty.ConsecutiveDay() *
                    distanceToRange( c.dayHigh[nextday] - c.dayLow[nextday] + 1,
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            }
        }
    }
    // handle last consecutive block
    int consecutiveDay = c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
    if (assign.isWorking( nurse, Weekday::Sun )) {
        if (c.isSingleConsecutiveDay()) {   // the entire week is one block
            if (history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                obj += penalty.ConsecutiveDay() * Weekday::NUM;
            } else {
                obj += penalty.ConsecutiveDay() * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayNum );
            }
        } else {
            obj += penalty.ConsecutiveDay() * exceedCount(
                consecutiveDay, contract.maxConsecutiveDayNum );
        }
    } else if (c.isSingleConsecutiveDay() // the entire week is one block
        && Assign::isWorking( history.lastShifts[nurse] )) {
        obj += penalty.ConsecutiveDay() * absentCount(
            history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
    }

    return obj;
}

NurseRostering::ObjValue NurseRostering::Solution::evaluateConsecutiveDayOff( NurseID nurse )
{
    ObjValue obj = 0;

    const History &history( problem.history );
    const Consecutive &c( consecutives[nurse] );
    const ContractID &contractID( problem.scenario.nurses[nurse].contract );
    const Scenario::Contract &contract( problem.scenario.contracts[contractID] );

    int nextday = c.dayHigh[Weekday::Mon] + 1;
    if (nextday <= Weekday::Sun) {   // the entire week is not one block
        // handle first block with history
        if (!assign.isWorking( nurse, Weekday::Mon )) {
            if (history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                obj += penalty.ConsecutiveDayOff() * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
            } else {
                obj += penalty.ConsecutiveDayOff() * distanceToRange(
                    c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            }
        } else if (!Assign::isWorking( history.lastShifts[nurse] )) {
            obj += penalty.ConsecutiveDayOff() * absentCount(
                history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
        }
        // handle blocks in the middle of the week
        for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
            if (!assign.isWorking( nurse, nextday )) {
                obj += penalty.ConsecutiveDayOff() *
                    distanceToRange( c.dayHigh[nextday] - c.dayLow[nextday] + 1,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            }
        }
    }
    // handle last consecutive block
    int consecutiveDay = c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
    if (!assign.isWorking( nurse, Weekday::Sun )) {
        if (c.isSingleConsecutiveDay()) {   // the entire week is one block
            if (history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                obj += penalty.ConsecutiveDayOff() * Weekday::NUM;
            } else {
                obj += penalty.ConsecutiveDayOff() * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayoffNum );
            }
        } else {
            obj += penalty.ConsecutiveDayOff() * exceedCount(
                consecutiveDay, contract.maxConsecutiveDayoffNum );
        }
    } else if (c.isSingleConsecutiveDay() // the entire week is one block
        && (!Assign::isWorking( history.lastShifts[nurse] ))) {
        obj += penalty.ConsecutiveDayOff() * absentCount(
            history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
    }

    return obj;
}

NurseRostering::ObjValue NurseRostering::Solution::evaluatePreference( NurseID nurse )
{
    ObjValue obj = 0;

    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        const ShiftID &shift = assign[nurse][weekday].shift;
        obj += penalty.Preference() *
            problem.weekData.shiftOffs[weekday][shift][nurse];
    }

    return obj;
}

NurseRostering::ObjValue NurseRostering::Solution::evaluateCompleteWeekend( NurseID nurse )
{
    ObjValue obj = 0;

    obj += penalty.CompleteWeekend() *
        (problem.scenario.contracts[problem.scenario.nurses[nurse].contract].completeWeekend
        && (assign.isWorking( nurse, Weekday::Sat ) != assign.isWorking( nurse, Weekday::Sun )));

    return obj;
}

NurseRostering::ObjValue NurseRostering::Solution::evaluateTotalAssign( NurseID nurse )
{
    ObjValue obj = 0;

    int min = problem.scenario.nurses[nurse].restMinShiftNum;
    int max = problem.scenario.nurses[nurse].restMaxShiftNum;
    obj += penalty.TotalAssign() * distanceToRange(
        totalAssignNums[nurse] * problem.history.restWeekCount,
        min, max ) / problem.history.restWeekCount;

    return obj;
}

NurseRostering::ObjValue NurseRostering::Solution::evaluateTotalWorkingWeekend( NurseID nurse )
{
    ObjValue obj = 0;

#ifdef INRC2_AVERAGE_MAX_WORKING_WEEKEND
    int maxWeekend = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
    int historyWeekend = problem.history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum;
    int exceedingWeekend = historyWeekend - (maxWeekend * problem.history.currentWeek) +
        ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * problem.scenario.totalWeekNum);
    if (exceedingWeekend > 0) {
        obj += penalty.TotalWorkingWeekend() *
            exceedingWeekend / problem.scenario.totalWeekNum;
    }
#else
    int workingWeekendNum = (assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun ));
    obj += penalty.TotalWorkingWeekend() * exceedCount(
        workingWeekendNum * problem.history.restWeekCount,
        problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
#endif

    return obj;
}



NurseRostering::Solution::AvailableNurses::AvailableNurses( const Solution &s )
    : sln( s ), nurseWithSkill( s.solver.getNurseWithSkill() )
{
}

void NurseRostering::Solution::AvailableNurses::setEnvironment( int w, SkillID s )
{
    weekday = w;
    skill = s;
    minSkillNum = 0;
    int size = nurseWithSkill[skill].size();
    validNurseNum_CurDay = vector<int>( size );
    validNurseNum_CurShift = vector<int>( size );
    for (int i = 0; i < size; ++i) {
        validNurseNum_CurDay[i] = nurseWithSkill[skill][i].size();
        validNurseNum_CurShift[i] = validNurseNum_CurDay[i];
    }
}

void NurseRostering::Solution::AvailableNurses::setShift( ShiftID s )
{
    shift = s;
    minSkillNum = 0;
    validNurseNum_CurShift = validNurseNum_CurDay;
}

NurseRostering::NurseID NurseRostering::Solution::AvailableNurses::getNurse()
{
    while (true) {
        // find nurses who have the required skill with minimum skill number
        while (true) {
            if (validNurseNum_CurShift[minSkillNum] > 0) {
                break;
            } else if (++minSkillNum == validNurseNum_CurShift.size()) {
                return NurseRostering::Scenario::Nurse::ID_NONE;
            }
        }

        // select one nurse from it
        while (true) {
            int n = sln.solver.randGen() % validNurseNum_CurShift[minSkillNum];
            NurseID nurse = nurseWithSkill[skill][minSkillNum][n];
            vector<NurseID> &nurseSet = nurseWithSkill[skill][minSkillNum];
            if (sln.getAssignTable().isWorking( nurse, weekday )) { // set the nurse invalid for current day
                swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
                swap( nurseSet[validNurseNum_CurShift[minSkillNum]], nurseSet[--validNurseNum_CurDay[minSkillNum]] );
            } else if (sln.isValidSuccession( nurse, shift, weekday )) {
                swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
                swap( nurseSet[validNurseNum_CurShift[minSkillNum]], nurseSet[--validNurseNum_CurDay[minSkillNum]] );
                return nurse;
            } else {    // set the nurse invalid for current shift
                swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
            }
            if (validNurseNum_CurShift[minSkillNum] == 0) {
                break;
            }
        }
    }
}
