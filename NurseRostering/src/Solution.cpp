#include "Solution.h"
#include "Solver.h"


using namespace std;


const NurseRostering::Solution::TryMoveTable
NurseRostering::Solution::tryMove = {
    &NurseRostering::Solution::tryAddAssign,
    &NurseRostering::Solution::tryRemoveAssign,
    &NurseRostering::Solution::tryChangeAssign,
    &NurseRostering::Solution::trySwapNurse,
    &NurseRostering::Solution::tryExchangeDay,
#ifdef INRC2_BLOCK_SWAP_FAST
    &NurseRostering::Solution::trySwapBlock_fast
#else
    &NurseRostering::Solution::trySwapBlock,
#endif
};
const NurseRostering::Solution::FindBestMoveTable
NurseRostering::Solution::findBestMove = {
    &NurseRostering::Solution::findBestAdd,
    &NurseRostering::Solution::findBestRemove,
    &NurseRostering::Solution::findBestChange,
    &NurseRostering::Solution::findBestSwap,
    &NurseRostering::Solution::findBestExchange,
#if INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_ORGN
    &NurseRostering::Solution::findBestBlockSwap,
#elif INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_FAST
    &NurseRostering::Solution::findBestBlockSwap_fast,
#elif INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_PART
    &NurseRostering::Solution::findBestBlockSwap_part,
#elif INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_RAND
    &NurseRostering::Solution::findBestBlockSwap_rand,
#endif
    &NurseRostering::Solution::findBestBlockShift,
    &NurseRostering::Solution::findBestARLoop,
    &NurseRostering::Solution::findBestARRand,
    &NurseRostering::Solution::findBestARBoth
};
const NurseRostering::Solution::FindBestMoveTable
NurseRostering::Solution::findBestMoveOnBlockBorder = {
    &NurseRostering::Solution::findBestAddOnBlockBorder,
    &NurseRostering::Solution::findBestRemoveOnBlockBorder,
    &NurseRostering::Solution::findBestChangeOnBlockBorder,
    &NurseRostering::Solution::findBestSwapOnBlockBorder,
    &NurseRostering::Solution::findBestExchangeOnBlockBorder,
#if INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_ORGN
    &NurseRostering::Solution::findBestBlockSwap,
#elif INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_FAST
    &NurseRostering::Solution::findBestBlockSwap_fast,
#elif INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_PART
    &NurseRostering::Solution::findBestBlockSwap_part,
#elif INRC2_BLOCK_SWAP_FIND_BEST == INRC2_BLOCK_SWAP_RAND
    &NurseRostering::Solution::findBestBlockSwap_rand,
#endif
    &NurseRostering::Solution::findBestBlockShift,
    &NurseRostering::Solution::findBestARLoopOnBlockBorder,
    &NurseRostering::Solution::findBestARRandOnBlockBorder,
    &NurseRostering::Solution::findBestARBothOnBlockBorder
};
const NurseRostering::Solution::ApplyMoveTable
NurseRostering::Solution::applyMove = {
    &NurseRostering::Solution::addAssign,
    &NurseRostering::Solution::removeAssign,
    &NurseRostering::Solution::changeAssign,
    &NurseRostering::Solution::swapNurse,
    &NurseRostering::Solution::exchangeDay,
    &NurseRostering::Solution::swapBlock
};
const NurseRostering::Solution::UpdateTabuTable
NurseRostering::Solution::updateTabuTable = {
    &NurseRostering::Solution::updateAddTabu,
    &NurseRostering::Solution::updateRemoveTabu,
    &NurseRostering::Solution::updateChangeTabu,
    &NurseRostering::Solution::updateSwapTabu,
    &NurseRostering::Solution::updateExchangeTabu,
    &NurseRostering::Solution::updateBlockSwapTabu
};

const vector<string> NurseRostering::Solution::modeSeqNames = {
    "[ARlCS]", "[ARrCS]", "[ARbCS]", "[ACSR]",
    "[ARlSCB]", "[ARrSCB]", "[ARbSCB]", "[ASCBR]",
    "[ARlCSE]", "[ARrCSE]", "[ARbCSE]", "[ACSER]",
    "[ARlCSEB]", "[ARrCSEB]", "[ARbCSEB]", "[ACSEBR]",
    "[ARlCB]", "[ARrCB]", "[ARbCB]", "[ACBR]",
    "[ARlCEB]", "[ARrCEB]", "[ARbCEB]", "[ACEBR]"
};
const vector<vector<int> > NurseRostering::Solution::modeSeqPatterns = {
    { Solution::Move::Mode::ARLoop, Solution::Move::Mode::Change, Solution::Move::Mode::Swap },
    { Solution::Move::Mode::ARRand, Solution::Move::Mode::Change, Solution::Move::Mode::Swap },
    { Solution::Move::Mode::ARBoth, Solution::Move::Mode::Change, Solution::Move::Mode::Swap },
    { Solution::Move::Mode::Add, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::Remove },

    { Solution::Move::Mode::ARLoop, Solution::Move::Mode::Swap, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARRand, Solution::Move::Mode::Swap, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARBoth, Solution::Move::Mode::Swap, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::Add, Solution::Move::Mode::Swap, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap, Solution::Move::Mode::Remove },

    { Solution::Move::Mode::ARLoop, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift },
    { Solution::Move::Mode::ARRand, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift },
    { Solution::Move::Mode::ARBoth, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift },
    { Solution::Move::Mode::Add, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift, Solution::Move::Mode::Remove },

    { Solution::Move::Mode::ARLoop, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARRand, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARBoth, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::Add, Solution::Move::Mode::Change, Solution::Move::Mode::Swap, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap, Solution::Move::Mode::Remove },

    { Solution::Move::Mode::ARLoop, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARRand, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARBoth, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::Add, Solution::Move::Mode::Change, Solution::Move::Mode::BlockSwap, Solution::Move::Mode::Remove },

    { Solution::Move::Mode::ARLoop, Solution::Move::Mode::Change, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARRand, Solution::Move::Mode::Change, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::ARBoth, Solution::Move::Mode::Change, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap },
    { Solution::Move::Mode::Add, Solution::Move::Mode::Change, Solution::Move::Mode::BlockShift, Solution::Move::Mode::BlockSwap, Solution::Move::Mode::Remove }
};

const double NurseRostering::Solution::NO_DIFF = -1;



NurseRostering::Solution::Solution( const TabuSolver &s )
    : solver( s ), problem( s.problem ), iterCount( 1 )
{
}

NurseRostering::Solution::Solution( const TabuSolver &s, const Output &output )
    : solver( s ), problem( s.problem ), iterCount( 1 )
{
    rebuild( output );
}

void NurseRostering::Solution::rebuild( const Output &output, double diff )
{
    int selectBound = static_cast<int>(diff * RAND_MAX);

    const AssignTable &assignTable( (&output.getAssignTable() != &assign)
        ? output.getAssignTable() : AssignTable( output.getAssignTable() ) );

    resetAssign();
    resetAssistData();

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            if (assignTable[nurse][weekday].isWorking()) {
                if (rand() >= selectBound) {
                    addAssign( weekday, nurse, assignTable[nurse][weekday] );
                }
            }
        }
    }

    if (selectBound > 0) {  // there may be difference
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

bool NurseRostering::Solution::genInitAssign( int greedyRetryCount )
{
    bool feasible;
    clock_t timeForBranchAndCut = solver.timer.restTime() * 3 / 4;
    do {
        feasible = genInitAssign_Greedy();
        if (!feasible) {
            Timer timer( (solver.timer.restTime() - timeForBranchAndCut) / greedyRetryCount );
            feasible = repair( timer );
        }
    } while (!feasible && (--greedyRetryCount > 0));

    return (feasible || genInitAssign_BranchAndCut());
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

bool NurseRostering::Solution::genInitAssign_BranchAndCut()
{
    resetAssign();

    bool feasible = fillAssign( Weekday::Mon, NurseRostering::Scenario::Shift::ID_BEGIN,
        NurseRostering::Scenario::Skill::ID_BEGIN, 0, 0 );

    rebuild();

    return feasible;
}

bool NurseRostering::Solution::fillAssign( int weekday, ShiftID shift, SkillID skill, NurseID nurse, int nurseNum )
{
    if (nurse >= problem.scenario.nurseNum) {
        if (nurseNum < problem.weekData.minNurseNums[weekday][shift][skill]) {
            return false;
        } else {
            return fillAssign( weekday, shift, skill + 1, 0, 0 );
        }
    } else if (skill >= problem.scenario.skillSize) {
        return fillAssign( weekday, shift + 1, NurseRostering::Scenario::Skill::ID_BEGIN, 0, 0 );
    } else if (shift >= problem.scenario.shiftSize) {
        return fillAssign( weekday + 1, NurseRostering::Scenario::Shift::ID_BEGIN,
            NurseRostering::Scenario::Skill::ID_BEGIN, 0, 0 );
    } else if (weekday > Weekday::Sun) {
        return true;
    }

    if (solver.timer.isTimeOut()) {
        return false;
    }

    Assign firstAssign( shift, skill );
    Assign secondAssign;
    NurseID firstNurseNum = nurseNum + 1;
    NurseID secondNurseNum = nurseNum;
    bool isNotAssignedBefore = !assign.isWorking( nurse, weekday );

    if (isNotAssignedBefore) {
        if (problem.scenario.nurses[nurse].skills[skill]
            && isValidSuccession( nurse, shift, weekday )) {
            if (rand() % 2) {
                swap( firstAssign, secondAssign );
                swap( firstNurseNum, secondNurseNum );
            }

            assign[nurse][weekday] = firstAssign;
            if (fillAssign( weekday, shift, skill, nurse + 1, firstNurseNum )) {
                return true;
            }
        }

        assign[nurse][weekday] = secondAssign;
    }

    if (fillAssign( weekday, shift, skill, nurse + 1, secondNurseNum )) {
        return true;
    } else if (isNotAssignedBefore) {
        assign[nurse][weekday] = Assign();
    }

    return false;
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
    // tabu table
    shiftTabu = ShiftTabu( problem.scenario.nurseNum,
        vector< vector< vector<IterCount> > >( Weekday::SIZE,
        vector< vector<IterCount> >( problem.scenario.shiftSize,
        vector<IterCount>( problem.scenario.skillSize, 0 ) ) ) );
    dayTabu = DayTabu( problem.scenario.nurseNum,
        vector<IterCount>( Weekday::SIZE, 0 ) );
    // flags
    findBestARLoop_flag = true;
    findBestARLoopOnBlockBorder_flag = true;
    findBestBlockSwap_startNurse = problem.scenario.nurseNum;
    isPossibilitySelect = false;
    isBlockSwapSelected = false;
}

void NurseRostering::Solution::evaluateObjValue( bool considerSpanningConstraint )
{
    objInsufficientStaff = 0;
    objConsecutiveShift = 0;
    objConsecutiveDay = 0;
    objConsecutiveDayOff = 0;
    objPreference = 0;
    objCompleteWeekend = 0;
    objTotalAssign = 0;
    objTotalWorkingWeekend = 0;

    evaluateInsufficientStaff();
    evaluateConsecutiveShift();
    evaluateConsecutiveDay();
    evaluateConsecutiveDayOff();
    evaluatePreference();
    evaluateCompleteWeekend();
    if (considerSpanningConstraint) {
        evaluateTotalAssign();
    }
    evaluateTotalWorkingWeekend();

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
        &NurseRostering::Solution::findBestChange
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
            const int deltaDecRatio = 16;   // = weights[mode] / weightDelta
            const int decError = -(deltaDecRatio - 1);
            vector<int> weights( modeNum, initWeight );
            int totalWeight = initWeight * modeNum;

            for (; !timer.isTimeOut() && (objValue > 0); ++iterCount) {
                int modeSelect = 0;
                for (int w = rand() % totalWeight; (w -= weights[modeSelect]) >= 0; ++modeSelect) {}

                Move bestMove;
                (this->*fbmt[modeSelect])(bestMove);

                int weightDelta;
                // update tabu list first because it requires original assignment
                (this->*updateTabuTable[bestMove.mode])(bestMove);
                (this->*applyMove[bestMove.mode])(bestMove);
                objValue += bestMove.delta;

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


void NurseRostering::Solution::swapChainSearch_DoubleHead( const Timer &timer, IterCount maxNoImproveChainLength )
{
    if (!((optima.getObjValue() < solver.getOptima().getObjValue())
        || (rand() % solver.PERTURB_ORIGIN_SELECT))
        || (optima.getObjValue() >= DefaultPenalty::MAX_OBJ_VALUE)) {
        optima = solver.getOptima();
    }

    while (true) {
        rebuild( optima );

        RandSelect<ObjValue> rs;
        Move bestMove;
        Move bestMoveForOneNurse;
        ObjValue bestDeltaForOneNurse = DefaultPenalty::FORBIDDEN_MOVE;

        // find start links for the chain of block swap
        penalty.setBlockSwapMode();
        Move move;
        for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
            for (move.nurse2 = move.nurse + 1; move.nurse2 < problem.scenario.nurseNum; ++move.nurse2) {
                if (solver.haveSameSkill( move.nurse, move.nurse2 )) {
                    for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
                        move.delta = trySwapBlock( move.weekday, move.weekday2, move.nurse, move.nurse2 );
                        if (move.delta < DefaultPenalty::MAX_OBJ_VALUE) {
                            bool isSwap = (nurseDelta > nurse2Delta);
                            if (isSwap) { swap( nurseDelta, nurse2Delta ); }
                            if (nurseDelta < 0) {
                                if (rs.isMinimal( move.delta, bestMove.delta )) {
                                    bestMove = move;
                                    if (isSwap) { swap( bestMove.nurse, bestMove.nurse2 ); }
                                }
                            } else if (nurseDelta < bestDeltaForOneNurse) { // in case no swap meet requirement above
                                bestMoveForOneNurse = move;
                                bestDeltaForOneNurse = nurseDelta;
                                if (isSwap) { swap( bestMoveForOneNurse.nurse, bestMoveForOneNurse.nurse2 ); }
                            }
                        }
                    }
                }
            }
        }
        penalty.setDefaultMode();

        if (bestMove.delta < DefaultPenalty::MAX_OBJ_VALUE) {
            if (genSwapChain( timer, bestMove, maxNoImproveChainLength )) {
                continue;
            } else {
                rebuild( optima );
            }
        }

        if (!genSwapChain( timer, bestMoveForOneNurse, maxNoImproveChainLength )) {
            return;
        }
    }
}

void NurseRostering::Solution::swapChainSearch( const Timer &timer, IterCount maxNoImproveChainLength )
{
    if (!((optima.getObjValue() < solver.getOptima().getObjValue())
        || (rand() % solver.PERTURB_ORIGIN_SELECT))
        || (optima.getObjValue() >= DefaultPenalty::MAX_OBJ_VALUE)) {
        optima = solver.getOptima();
    }

    priority_queue<Move> bestMoves;

    IterCount count = solver.MaxSwapChainRestartCount();
    while ((--count) > 0) {
        rebuild( optima );

        // find start links for the chain of block swap
        if (bestMoves.empty()) {
            penalty.setBlockSwapMode();
            Move move;
            for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
                for (move.nurse2 = move.nurse + 1; move.nurse2 < problem.scenario.nurseNum; ++move.nurse2) {
                    if (solver.haveSameSkill( move.nurse, move.nurse2 )) {
                        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
                            move.delta = trySwapBlock( move.weekday, move.weekday2, move.nurse, move.nurse2 );
                            if (move.delta < DefaultPenalty::MAX_OBJ_VALUE) {
                                bool isSwap = (nurseDelta > nurse2Delta);
                                if (isSwap) { swap( nurseDelta, nurse2Delta ); }
                                if (nurseDelta < 0) {
                                    if (isSwap) { swap( move.nurse, move.nurse2 ); }
                                    bestMoves.push( move );
                                    if (isSwap) { swap( move.nurse, move.nurse2 ); }
                                }
                            }
                        }
                    }
                }
            }
            penalty.setDefaultMode();
        }

        Move head;
        if (bestMoves.empty()) {
            return;
        } else {
            head = bestMoves.top();
            bestMoves.pop();
        }
        if (genSwapChain( timer, head, maxNoImproveChainLength )) {
            count = solver.MaxSwapChainRestartCount();
            bestMoves = priority_queue<Move>();
        }
    }
}

bool NurseRostering::Solution::genSwapChain( const Timer &timer, const Move &head, IterCount noImproveLen )
{
    RandSelect<ObjValue> rs;
    Move bestMove( head );
    Move move;

    // try to improve a chain of worsened nurse
    IterCount len = noImproveLen;
    for (; !timer.isTimeOut() && (len > 0); ++iterCount) {
        swapBlock( bestMove );
        objValue += bestMove.delta;
        if (updateOptima()) { return true; }

        // try to improve the worsened nurse
        bestMove.delta = DefaultPenalty::FORBIDDEN_MOVE;
        move.nurse = bestMove.nurse2;
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                move.delta = tryRemoveAssign( move );
                if (rs.isMinimal( move.delta, bestMove.delta )) {
                    bestMove = move;
                    bestMove.mode = Move::Mode::Remove;
                }
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryChangeAssign( move );
                        if (rs.isMinimal( move.delta, bestMove.delta )) {
                            bestMove = move;
                            bestMove.mode = Move::Mode::Change;
                        }
                    }
                }
            } else {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryAddAssign( move );
                        if (rs.isMinimal( move.delta, bestMove.delta )) {
                            bestMove = move;
                            bestMove.mode = Move::Mode::Add;
                        }
                    }
                }
            }
        }
        penalty.setExchangeMode();
        const Consecutive &c( consecutives[move.nurse] );
        move.weekday = Weekday::Mon;
        move.weekday2 = c.dayHigh[move.weekday] + 1;
        while (move.weekday2 <= Weekday::Sun) {
            move.delta = tryExchangeDay( move.weekday, move.nurse, move.weekday2 );
            if (rs.isMinimal( move.delta, bestMove.delta )) {
                bestMove = move;
                bestMove.mode = Move::Mode::Exchange;
            }
            if (move.weekday != c.dayHigh[move.weekday]) {  // start of a block
                move.weekday = c.dayHigh[move.weekday];
                move.weekday2 = c.dayHigh[move.weekday + 1];
            } else {    // end of a block
                ++move.weekday;
                move.weekday2 = c.dayHigh[move.weekday] + 1;
            }
        }
        penalty.setDefaultMode();

        // apply add/change/remove/shift if there is improvement
#ifdef INRC2_SWAP_CHAIN_MAKE_BAD_MOVE
        if (bestMove.delta < 0) {
#else
        if (objValue + bestMove.delta < optima.getObjValue()) {
#endif
            (this->*applyMove[bestMove.mode])(bestMove);
            objValue += bestMove.delta;
            if (updateOptima()) { return true; }
        }

        // find next link
        penalty.setBlockSwapMode();
        bestMove.delta = DefaultPenalty::FORBIDDEN_MOVE;
#ifdef INRC2_SWAP_CHAIN_MAKE_BAD_MOVE
        Move bestMoveForOneNurse;
        ObjValue bestDeltaForOneNurse = DefaultPenalty::FORBIDDEN_MOVE;
#endif
        for (move.nurse2 = 0; move.nurse2 < problem.scenario.nurseNum; ++move.nurse2) {
            if ((move.nurse != move.nurse2) && solver.haveSameSkill( move.nurse, move.nurse2 )) {
                for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
                    if (!(isValidSuccession( move.nurse, assign[move.nurse2][move.weekday].shift, move.weekday )
                        && isValidSuccession( move.nurse2, assign[move.nurse][move.weekday].shift, move.weekday ))) {
                        continue;
                    }

                    move.delta = 0;
                    ObjValue headDelta = 0;
                    move.weekday2 = move.weekday;
                    // try each block length
                    while (problem.scenario.nurses[move.nurse].skills[assign[move.nurse2][move.weekday2].skill]
                        && problem.scenario.nurses[move.nurse2].skills[assign[move.nurse][move.weekday2].skill]) {
                        // longer blocks will also miss this skill
                        move.delta += trySwapNurse( move.weekday2, move.nurse, move.nurse2 );
                        headDelta += nurseDelta;

                        if (move.delta < DefaultPenalty::MAX_OBJ_VALUE) {
                            if (isValidPrior( move.nurse, assign[move.nurse2][move.weekday2].shift, move.weekday2 )
                                && isValidPrior( move.nurse2, assign[move.nurse][move.weekday2].shift, move.weekday2 )) {
                                if ((objValue + headDelta < optima.getObjValue())
                                    || (objValue + move.delta < optima.getObjValue())) {
                                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                                        bestMove = move;
                                    }
#ifdef INRC2_SWAP_CHAIN_MAKE_BAD_MOVE
                                } else if (headDelta < bestDeltaForOneNurse) { // in case no swap meet requirement above
                                    bestMoveForOneNurse = move;
                                    bestDeltaForOneNurse = nurseDelta;
#endif
                                }
                            }
                        } else {    // two day off
                            move.delta -= DefaultPenalty::FORBIDDEN_MOVE;
                        }

                        if (move.weekday2 >= Weekday::Sun) { break; }

                        (const_cast<Solution*>(this))->swapNurse( move.weekday2, move.nurse, move.nurse2 );
                        ++move.weekday2;
                    }

                    // recover original data
                    while ((--move.weekday2) >= move.weekday) {
                        (const_cast<Solution*>(this))->swapNurse( move.weekday2, move.nurse, move.nurse2 );
                    }
                }
            }
        }
        penalty.setDefaultMode();

#ifdef INRC2_SWAP_CHAIN_MAKE_BAD_MOVE
        if (bestMove.delta >= DefaultPenalty::MAX_OBJ_VALUE) {
            bestMove = bestMoveForOneNurse;
        }
#endif
        if (bestMove.delta >= DefaultPenalty::MAX_OBJ_VALUE) {
            return false;
        }

        (objValue + bestMove.delta < optima.getObjValue())
            ? (len = noImproveLen) : (--len);
    }

    return false;
}

void NurseRostering::Solution::tabuSearch_Rand( const Timer &timer, const FindBestMoveTable &findBestMoveTable )
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
    const int deltaDecRatio = 16;   // = weights[mode] / weightDelta
    const int decError = -(deltaDecRatio - 1);
    vector<int> weights( modeNum, initWeight );
    int totalWeight = initWeight * modeNum;

    IterCount noImprove = solver.MaxNoImproveForAllNeighborhood();
    for (; !timer.isTimeOut() && (noImprove > 0); ++iterCount) {
        int modeSelect = 0;
        for (int w = rand() % totalWeight; (w -= weights[modeSelect]) >= 0; ++modeSelect) {}

        Move bestMove;
        (this->*findBestMoveTable[modeSelect])(bestMove);

        int weightDelta;
        if (bestMove.delta < DefaultPenalty::MAX_OBJ_VALUE) {
            // update tabu list first because it requires original assignment
            (this->*updateTabuTable[bestMove.mode])(bestMove);
            (this->*applyMove[bestMove.mode])(bestMove);
            objValue += bestMove.delta;

            if (updateOptima()) {   // improve optima
#ifdef INRC2_LS_AFTER_TSR_UPDATE_OPT
                localSearch( timer, findBestMoveTable );
#endif
                noImprove = solver.MaxNoImproveForAllNeighborhood();
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

void NurseRostering::Solution::tabuSearch_Loop( const Timer &timer, const FindBestMoveTable &findBestMoveTable )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
    IterCount startIterCount = iterCount;
#endif
    optima = *this;

    int modeNum = findBestMoveTable.size();

    int failCount = modeNum;
    int modeSelect = 0;
    while (!timer.isTimeOut()) {
        // reset current solution to best solution found in last neighborhood
        rebuild( optima );

        IterCount noImprove_Single = solver.MaxNoImproveForSingleNeighborhood();
        for (; !timer.isTimeOut() && (noImprove_Single > 0); ++iterCount) {
            Move bestMove;
            (this->*findBestMoveTable[modeSelect])(bestMove);

            if (bestMove.delta >= DefaultPenalty::MAX_OBJ_VALUE) {
                break;
            }

            // update tabu list first because it requires original assignment
            (this->*updateTabuTable[bestMove.mode])(bestMove);
            (this->*applyMove[bestMove.mode])(bestMove);
            objValue += bestMove.delta;

            if (updateOptima()) {   // improved
                failCount = modeNum;
                noImprove_Single = solver.MaxNoImproveForSingleNeighborhood();
            } else {    // not improved
                --noImprove_Single;
            }
        }

        // since there is randomness on the search trajectory,
        // there will be chance to make a difference on neighborhoods
        // which have been searched. so search two more times
        if (failCount >= 0) {   // repeat (modeNum + 2)
            --failCount;
            (++modeSelect) %= modeNum;
        } else {
            break;
        }
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cout << "[TS] iter: " << (iterCount - startIterCount) << ' '
        << "time: " << duration << ' '
        << "speed: " << (iterCount - startIterCount) * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::Solution::tabuSearch_Possibility( const Timer &timer, const FindBestMoveTable &findBestMoveTable )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
    IterCount startIterCount = iterCount;
#endif
    isPossibilitySelect = true;

    optima = *this;

    int modeNum = findBestMoveTable.size();
    int startMode = 0;

    const int maxP_local = RAND_MAX / modeNum;
    const int maxP_global = RAND_MAX * (modeNum - 1) / modeNum;
    const double amp_local = 1.0 / (2 * modeNum);
    const double amp_global = 1.0 / (4 * modeNum * modeNum);
    const double dec_local = (2.0 * modeNum - 1) / (2 * modeNum);
    const double dec_global = (2.0 * modeNum * modeNum - 1) / (2 * modeNum * modeNum);
    int P_global = RAND_MAX / modeNum;
    vector<int> P_local( modeNum, 0 );

    IterCount noImprove = solver.MaxNoImproveForAllNeighborhood();
    for (; !timer.isTimeOut() && (noImprove > 0); ++iterCount) {
        int modeSelect = startMode;
        Move::Mode moveMode = Move::Mode::SIZE;
        Move bestMove;
        isBlockSwapSelected = false;
        // judge every neighborhood whether to select and search when selected
        // start from big end to make sure block swap will be tested before swap
        for (int i = modeNum - 1; i >= 0; --i) {
            if (rand() < (P_global + P_local[i])) { // selected
                (this->*findBestMoveTable[i])(bestMove);
                if (moveMode != bestMove.mode) {
                    moveMode = bestMove.mode;
                    modeSelect = i;
                }
            }
        }

        // no one is selected
        while (bestMove.delta >= DefaultPenalty::MAX_OBJ_VALUE) {
            (this->*findBestMoveTable[modeSelect])(bestMove);
            modeSelect += (bestMove.delta >= DefaultPenalty::MAX_OBJ_VALUE);
            modeSelect %= modeNum;
        }

        // update tabu list first because it requires original assignment
        (this->*updateTabuTable[bestMove.mode])(bestMove);
        (this->*applyMove[bestMove.mode])(bestMove);
        objValue += bestMove.delta;

        if (updateOptima()) {   // improved
            noImprove = solver.MaxNoImproveForAllNeighborhood();
            P_global = static_cast<int>(P_global * dec_global);
            P_local[modeSelect] += static_cast<int>(amp_local * (maxP_local - P_local[modeSelect]));
        } else {    // not improved
            --noImprove;
            (++startMode) %= modeNum;
            P_global += static_cast<int>(amp_global * (maxP_global - P_global));
            P_local[modeSelect] = static_cast<int>(P_local[modeSelect] * dec_local);
        }
    }

    isPossibilitySelect = false;
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cout << "[TS] iter: " << (iterCount - startIterCount) << ' '
        << "time: " << duration << ' '
        << "speed: " << (iterCount - startIterCount) * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::Solution::localSearch( const Timer &timer, const FindBestMoveTable &findBestMoveTable )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
    IterCount startIterCount = iterCount;
#endif
    optima = *this;

    int modeNum = findBestMoveTable.size();

    int failCount = modeNum;
    int modeSelect = 0;
    while (!timer.isTimeOut() && (failCount > 0)) {
        Move bestMove;
        if ((this->*findBestMoveTable[modeSelect])(bestMove)) {
            (this->*applyMove[bestMove.mode])(bestMove);
            objValue += bestMove.delta;
            updateOptima();
            ++iterCount;
            failCount = modeNum;
        } else {
            --failCount;
            (++modeSelect) %= modeNum;
        }
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cout << "[LS] iter: " << (iterCount - startIterCount) << ' '
        << "time: " << duration << ' '
        << "speed: " << (iterCount - startIterCount) * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::Solution::randomWalk( const Timer &timer, IterCount stepNum )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
    IterCount startIterCount = stepNum;
#endif
    optima = *this;

    ObjValue delta;
    while ((stepNum > 0) && !timer.isTimeOut()) {
        int moveMode = rand() % Move::Mode::BASIC_MOVE_SIZE;
        Move move;
        move.weekday = (rand() % Weekday::NUM) + Weekday::Mon;
        move.weekday2 = (rand() % Weekday::NUM) + Weekday::Mon;
        if (move.weekday > move.weekday2) { swap( move.weekday, move.weekday2 ); }
        move.nurse = rand() % problem.scenario.nurseNum;
        move.nurse2 = rand() % problem.scenario.nurseNum;
        move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN +
            (rand() % problem.scenario.shiftTypeNum);
        move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN +
            (rand() % problem.scenario.skillTypeNum);

        delta = (this->*tryMove[moveMode])(move);
        if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
            (this->*applyMove[moveMode])(move);
            objValue += delta;
            --stepNum;
        }
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cout << "[RW] iter: " << startIterCount << ' '
        << "time: " << duration << ' '
        << "speed: " << startIterCount * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::Solution::perturb( double strength )
{
    // TODO : make this change solution structure in certain complexity
    int randomWalkStepCount = static_cast<int>(strength *
        problem.scenario.shiftTypeNum * problem.scenario.nurseNum * Weekday::NUM);

    randomWalk( solver.timer, randomWalkStepCount );

    updateOptima();
}


bool NurseRostering::Solution::findBestAdd( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

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
                        if (noAddTabu( move )) {
                            if (rs.isMinimal( move.delta, bestMove.delta )) {
                                bestMove = move;
                            }
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                                bestMove_tabu = move;
                            }
                        }
                    }
                }
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestChange( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

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
                        if (noChangeTabu( move )) {
                            if (rs.isMinimal( move.delta, bestMove.delta )) {
                                bestMove = move;
                            }
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                                bestMove_tabu = move;
                            }
                        }
                    }
                }
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestRemove( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::Remove;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                move.delta = tryRemoveAssign( move );
                if (noRemoveTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                    }
                }
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestSwap( Move &bestMove ) const
{
    if (isPossibilitySelect && isBlockSwapSelected) {
        return false;
    }

    penalty.setSwapMode();

    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::Swap;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.nurse2 = move.nurse + 1; move.nurse2 < problem.scenario.nurseNum; ++move.nurse2) {
            for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
                move.delta = trySwapNurse( move.weekday, move.nurse, move.nurse2 );
                if (noSwapTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                    }
                }
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestBlockSwap( Move &bestMove ) const
{
    const NurseID maxNurseID = problem.scenario.nurseNum - 1;

    isBlockSwapSelected = true;
    penalty.setBlockSwapMode();

    RandSelect<ObjValue> rs;

    Move move;
    move.mode = Move::Mode::BlockSwap;
    move.nurse = findBestBlockSwap_startNurse;
    for (NurseID count = problem.scenario.nurseNum; count > 0; --count) {
        (move.nurse < maxNurseID) ? (++move.nurse) : (move.nurse = 0);
        move.nurse2 = move.nurse;
        for (NurseID count2 = count - 1; count2 > 0; --count2) {
            (move.nurse2 < maxNurseID) ? (++move.nurse2) : (move.nurse2 = 0);
            if (solver.haveSameSkill( move.nurse, move.nurse2 )) {
                for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
                    move.delta = trySwapBlock( move.weekday, move.weekday2, move.nurse, move.nurse2 );
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
#ifdef INRC2_BLOCK_SWAP_FIRST_IMPROVE
                        if (bestMove.delta < 0) {
                            findBestBlockSwap_startNurse = move.nurse;
                            penalty.setDefaultMode();
                            return true;
                        }
#endif
                    }
                }
            }
        }
    }

    findBestBlockSwap_startNurse = move.nurse;
    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestBlockSwap_fast( Move &bestMove ) const
{
    const NurseID maxNurseID = problem.scenario.nurseNum - 1;

    isBlockSwapSelected = true;
    penalty.setBlockSwapMode();

    RandSelect<ObjValue> rs;

    Move move;
    move.mode = Move::Mode::BlockSwap;
    move.nurse = findBestBlockSwap_startNurse;
    for (NurseID count = problem.scenario.nurseNum; count > 0; --count) {
        (move.nurse < maxNurseID) ? (++move.nurse) : (move.nurse = 0);
        move.nurse2 = move.nurse;
        for (NurseID count2 = count - 1; count2 > 0; --count2) {
            (move.nurse2 < maxNurseID) ? (++move.nurse2) : (move.nurse2 = 0);
            if (solver.haveSameSkill( move.nurse, move.nurse2 )) {
                move.delta = trySwapBlock_fast( move.weekday, move.weekday2, move.nurse, move.nurse2 );
                if (rs.isMinimal( move.delta, bestMove.delta )) {
                    bestMove = move;
#ifdef INRC2_BLOCK_SWAP_FIRST_IMPROVE
                    if (bestMove.delta < 0) {
                        findBestBlockSwap_startNurse = move.nurse;
                        penalty.setDefaultMode();
                        return true;
                    }
#endif
                }
            }
        }
    }

    findBestBlockSwap_startNurse = move.nurse;
    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestBlockSwap_part( Move &bestMove ) const
{
    const int nurseNum_noTry = problem.scenario.nurseNum - problem.scenario.nurseNum / 4;
    const NurseID maxNurseID = problem.scenario.nurseNum - 1;

    isBlockSwapSelected = true;
    penalty.setBlockSwapMode();

    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::BlockSwap;
    move.nurse = findBestBlockSwap_startNurse;
    for (NurseID count = problem.scenario.nurseNum; count > nurseNum_noTry; --count) {
        (move.nurse < maxNurseID) ? (++move.nurse) : (move.nurse = 0);
        move.nurse2 = move.nurse;
        for (NurseID count2 = count - 1; count2 > 0; --count2) {
            (move.nurse2 < maxNurseID) ? (++move.nurse2) : (move.nurse2 = 0);
            if (solver.haveSameSkill( move.nurse, move.nurse2 )) {
                move.delta = trySwapBlock_fast( move.weekday, move.weekday2, move.nurse, move.nurse2 );
                if (rs.isMinimal( move.delta, bestMove.delta )) {
                    bestMove = move;
#ifdef INRC2_BLOCK_SWAP_FIRST_IMPROVE
                    if (bestMove.delta < 0) {
                        findBestBlockSwap_startNurse = move.nurse;
                        penalty.setDefaultMode();
                        return true;
                    }
#endif
                }
            }
        }
    }

    findBestBlockSwap_startNurse = move.nurse;
    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestBlockSwap_rand( Move &bestMove ) const
{
    const int nurseNum_noTry = problem.scenario.nurseNum - problem.scenario.nurseNum / 4;
    const NurseID maxNurseID = problem.scenario.nurseNum - 1;

    isBlockSwapSelected = true;
    penalty.setBlockSwapMode();

    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::BlockSwap;
    for (NurseID count = problem.scenario.nurseNum; count > nurseNum_noTry; --count) {
        move.nurse = rand() % problem.scenario.nurseNum;
        move.nurse2 = move.nurse;
        for (NurseID count2 = count - 1; count2 > 0; --count2) {
            (move.nurse2 < maxNurseID) ? (++move.nurse2) : (move.nurse2 = 0);
            if (solver.haveSameSkill( move.nurse, move.nurse2 )) {
                move.delta = trySwapBlock_fast( move.weekday, move.weekday2, move.nurse, move.nurse2 );
                if (rs.isMinimal( move.delta, bestMove.delta )) {
                    bestMove = move;
#ifdef INRC2_BLOCK_SWAP_FIRST_IMPROVE
                    if (bestMove.delta < 0) {
                        findBestBlockSwap_startNurse = move.nurse;
                        penalty.setDefaultMode();
                        return true;
                    }
#endif
                }
            }
        }
    }

    findBestBlockSwap_startNurse = move.nurse;
    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestExchange( Move &bestMove ) const
{
    penalty.setExchangeMode();

    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::Exchange;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            for (move.weekday2 = move.weekday + 1; move.weekday2 <= Weekday::Sun; ++move.weekday2) {
                move.delta = tryExchangeDay( move.weekday, move.nurse, move.weekday2 );
                if (noExchangeTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                    }
                }
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestBlockShift( Move &bestMove ) const
{
    penalty.setExchangeMode();

    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::Exchange;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        const Consecutive &c( consecutives[move.nurse] );
        move.weekday = Weekday::Mon;
        move.weekday2 = c.dayHigh[move.weekday] + 1;
        while (move.weekday2 <= Weekday::Sun) {
            move.delta = tryExchangeDay( move.weekday, move.nurse, move.weekday2 );
            if (noExchangeTabu( move )) {
                if (rs.isMinimal( move.delta, bestMove.delta )) {
                    bestMove = move;
                }
            } else {    // tabu
                if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                    bestMove_tabu = move;
                }
            }
            if (move.weekday != c.dayHigh[move.weekday]) {  // start of a block
                move.weekday = c.dayHigh[move.weekday];
                move.weekday2 = c.dayHigh[move.weekday + 1];
            } else {    // end of a block
                ++move.weekday;
                move.weekday2 = c.dayHigh[move.weekday] + 1;
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestARLoop( Move &bestMove ) const
{
    bool isImproved;
    isImproved = findBestARLoop_flag
        ? findBestAdd( bestMove ) : findBestRemove( bestMove );

    if (!isImproved) {
        findBestARLoop_flag = !findBestARLoop_flag;
    }

    return isImproved;
}

bool NurseRostering::Solution::findBestARRand( Move &bestMove ) const
{
    return ((rand() % 2) ? findBestAdd( bestMove ) : findBestRemove( bestMove ));
}

bool NurseRostering::Solution::findBestARBoth( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun; ++move.weekday) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                move.delta = tryRemoveAssign( move );
                if (noRemoveTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                        bestMove.mode = Move::Mode::Remove;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                        bestMove_tabu.mode = Move::Mode::Remove;
                    }
                }
            } else {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryAddAssign( move );
                        if (noAddTabu( move )) {
                            if (rs.isMinimal( move.delta, bestMove.delta )) {
                                bestMove = move;
                                bestMove.mode = Move::Mode::Add;
                            }
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                                bestMove_tabu = move;
                                bestMove_tabu.mode = Move::Mode::Add;
                            }
                        }
                    }
                }
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestAddOnBlockBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

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
                        if (noAddTabu( move )) {
                            if (rs.isMinimal( move.delta, bestMove.delta )) {
                                bestMove = move;
                            }
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                                bestMove_tabu = move;
                            }
                        }
                    }
                }
                move.weekday = (move.weekday != c.dayHigh[move.weekday]) ? c.dayHigh[move.weekday] : (move.weekday + 1);
            } else {
                move.weekday = c.dayHigh[move.weekday] + 1;
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestChangeOnBlockBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

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
                        if (noChangeTabu( move )) {
                            if (rs.isMinimal( move.delta, bestMove.delta )) {
                                bestMove = move;
                            }
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                                bestMove_tabu = move;
                            }
                        }
                    }
                }
                move.weekday = (move.weekday != c.shiftHigh[move.weekday]) ? c.shiftHigh[move.weekday] : (move.weekday + 1);
            } else {
                move.weekday = c.shiftHigh[move.weekday] + 1;
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestRemoveOnBlockBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::Remove;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        const Consecutive &c( consecutives[move.nurse] );
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun;) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                move.delta = tryRemoveAssign( move );
                if (noRemoveTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                    }
                }
                move.weekday = (move.weekday != c.dayHigh[move.weekday]) ? c.dayHigh[move.weekday] : (move.weekday + 1);
            } else {
                move.weekday = c.dayHigh[move.weekday] + 1;
            }
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestSwapOnBlockBorder( Move &bestMove ) const
{
    if (isPossibilitySelect && isBlockSwapSelected) {
        return false;
    }

    penalty.setSwapMode();

    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::Swap;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        const Consecutive &c( consecutives[move.nurse] );
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun;) {
            for (move.nurse2 = 0; move.nurse2 < problem.scenario.nurseNum; ++move.nurse2) {
                move.delta = trySwapNurse( move.weekday, move.nurse, move.nurse2 );
                if (noSwapTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                    }
                }
            }
            move.weekday = (move.weekday != c.dayHigh[move.weekday]) ? c.dayHigh[move.weekday] : (move.weekday + 1);
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestExchangeOnBlockBorder( Move &bestMove ) const
{
    penalty.setExchangeMode();

    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    move.mode = Move::Mode::Exchange;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        const Consecutive &c( consecutives[move.nurse] );
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun;) {
            for (move.weekday2 = Weekday::Mon; move.weekday2 <= Weekday::Sun; ++move.weekday2) {
                move.delta = tryExchangeDay( move.weekday, move.nurse, move.weekday2 );
                if (noExchangeTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                    }
                }
            }
            move.weekday = (move.weekday != c.dayHigh[move.weekday]) ? c.dayHigh[move.weekday] : (move.weekday + 1);
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

    penalty.setDefaultMode();
    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestARLoopOnBlockBorder( Move &bestMove ) const
{
    bool isImproved;
    isImproved = findBestARLoopOnBlockBorder_flag
        ? findBestAddOnBlockBorder( bestMove ) : findBestRemove( bestMove );

    if (!isImproved) {
        findBestARLoopOnBlockBorder_flag = !findBestARLoopOnBlockBorder_flag;
    }

    return isImproved;
}

bool NurseRostering::Solution::findBestARRandOnBlockBorder( Move &bestMove ) const
{
    return ((rand() % 2) ? findBestAddOnBlockBorder( bestMove ) : findBestRemoveOnBlockBorder( bestMove ));
}

bool NurseRostering::Solution::findBestARBothOnBlockBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    Move bestMove_tabu;
    RandSelect<ObjValue> rs_tabu;

    Move move;
    for (move.nurse = 0; move.nurse < problem.scenario.nurseNum; ++move.nurse) {
        const Consecutive &c( consecutives[move.nurse] );
        for (move.weekday = Weekday::Mon; move.weekday <= Weekday::Sun;) {
            if (assign.isWorking( move.nurse, move.weekday )) {
                move.delta = tryRemoveAssign( move );
                if (noRemoveTabu( move )) {
                    if (rs.isMinimal( move.delta, bestMove.delta )) {
                        bestMove = move;
                        bestMove.mode = Move::Mode::Remove;
                    }
                } else {    // tabu
                    if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                        bestMove_tabu = move;
                        bestMove_tabu.mode = Move::Mode::Remove;
                    }
                }
            } else {
                for (move.assign.shift = NurseRostering::Scenario::Shift::ID_BEGIN;
                    move.assign.shift < problem.scenario.shiftSize; ++move.assign.shift) {
                    for (move.assign.skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                        move.assign.skill < problem.scenario.skillSize; ++move.assign.skill) {
                        move.delta = tryAddAssign( move );
                        if (noAddTabu( move )) {
                            if (rs.isMinimal( move.delta, bestMove.delta )) {
                                bestMove = move;
                                bestMove.mode = Move::Mode::Add;
                            }
                        } else {    // tabu
                            if (rs_tabu.isMinimal( move.delta, bestMove_tabu.delta )) {
                                bestMove_tabu = move;
                                bestMove_tabu.mode = Move::Mode::Add;
                            }
                        }
                    }
                }
            }
            move.weekday = (move.weekday != c.dayHigh[move.weekday]) ? c.dayHigh[move.weekday] : (move.weekday + 1);
        }
    }

    if (aspirationCritiera( bestMove.delta, bestMove_tabu.delta )) {
        bestMove = bestMove_tabu;
    }

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
    int currentWeek = problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta -= penalty.InsufficientStaff() *
        (missingNurseNums[weekday][a.shift][a.skill] > 0);

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
            const History &history( problem.history );
            delta -= penalty.TotalWorkingWeekend() * exceedCount(
                history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
            delta += penalty.TotalWorkingWeekend() * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
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

    return delta;
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

    return delta;
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
    int currentWeek = problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta += penalty.InsufficientStaff() *
        (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);

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
            const History &history( problem.history );
            delta -= penalty.TotalWorkingWeekend() * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
            delta += penalty.TotalWorkingWeekend() * exceedCount(
                history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / problem.scenario.totalWeekNum;
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

    return delta;
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
            nurse2Delta = ((nurseDelta < DefaultPenalty::MAX_OBJ_VALUE)
                ? tryChangeAssign( weekday, nurse2, assign[nurse][weekday] ) : 0);
        } else {
            nurseDelta = tryRemoveAssign( weekday, nurse );
            nurse2Delta = ((nurseDelta < DefaultPenalty::MAX_OBJ_VALUE)
                ? tryAddAssign( weekday, nurse2, assign[nurse][weekday] ) : 0);
        }
    } else {
        if (assign.isWorking( nurse2, weekday )) {
            nurseDelta = tryAddAssign( weekday, nurse, assign[nurse2][weekday] );
            nurse2Delta = ((nurseDelta < DefaultPenalty::MAX_OBJ_VALUE)
                ? tryRemoveAssign( weekday, nurse2 ) : 0);
        } else {    // no change
            nurseDelta = 0;
            nurse2Delta = 0;
            return DefaultPenalty::FORBIDDEN_MOVE;
        }
    }

    return (nurseDelta + nurse2Delta);
}

NurseRostering::ObjValue NurseRostering::Solution::trySwapNurse( const Move &move ) const
{
    penalty.setSwapMode();
    ObjValue delta = trySwapNurse( move.weekday, move.nurse, move.nurse2 );
    penalty.setDefaultMode();

    return delta;
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
                    if (rs.isMinimal( delta, minDelta )) {
                        minDelta = delta;
                        weekday2 = w;
                        minNurseDelta = delta1;
                        minNurse2Delta = delta2;
                    }
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
                } else {    // tabu
                    if (rs_tabu.isMinimal( delta, minDelta_tabu )) {
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

NurseRostering::ObjValue NurseRostering::Solution::trySwapBlock_fast( int &weekday, int &weekday2, NurseID nurse, NurseID nurse2 ) const
{
    // TODO : make sure they won't be the same and leave out this
    if (nurse == nurse2) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    RandSelect<ObjValue> rs;
    ObjValue delta = 0;
    ObjValue delta1 = 0;
    ObjValue delta2 = 0;
    ObjValue minDelta = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurseDelta = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurse2Delta = DefaultPenalty::FORBIDDEN_MOVE;

    // prepare for hard constraint check and tabu judgment
    bool hasSkill[Weekday::SIZE];
    for (int w = Weekday::Mon; w <= Weekday::Sun; ++w) {
        hasSkill[w] = (problem.scenario.nurses[nurse].skills[assign[nurse2][w].skill]
            && problem.scenario.nurses[nurse2].skills[assign[nurse][w].skill]);
    }

    weekday = Weekday::Mon;
    weekday2 = Weekday::Mon;

#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
    RandSelect<ObjValue> rs_tabu;
    ObjValue minDelta_tabu = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurseDelta_tabu = DefaultPenalty::FORBIDDEN_MOVE;
    ObjValue minNurse2Delta_tabu = DefaultPenalty::FORBIDDEN_MOVE;

    int noTabuCount[Weekday::SIZE][Weekday::SIZE];
    bool noTabu[Weekday::SIZE][Weekday::SIZE];
    for (int w = Weekday::Mon; w <= Weekday::Sun; ++w) {
        noTabuCount[w][w] = noSwapTabu( w, nurse, nurse2 );
        noTabu[w][w] = (noTabuCount[w][w] > 0);
    }
    for (int w = Weekday::Mon; w <= Weekday::Sun; ++w) {
        for (int w2 = w + 1; w2 <= Weekday::Sun; ++w2) {
            noTabuCount[w][w2] = noTabuCount[w][w2 - 1] + noTabuCount[w2][w2];
            noTabu[w][w2] = noBlockSwapTabu( noTabuCount[w][w2], w2 - w + 1 );
        }
    }

    int weekday_tabu = weekday;
    int weekday2_tabu = weekday2;
#endif

    // try each block length
    int w = Weekday::Mon;
    int w2;
    for (; w <= Weekday::Sun; ++w) {
        if (!(isValidSuccession( nurse, assign[nurse2][w].shift, w )
            && isValidSuccession( nurse2, assign[nurse][w].shift, w ))) {
            continue;
        }

        w2 = w;
        for (; (w2 <= Weekday::Sun) && hasSkill[w2]; ++w2) { // longer blocks will also miss this skill
            delta += trySwapNurse( w2, nurse, nurse2 );
            delta1 += nurseDelta;
            delta2 += nurse2Delta;

            if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                (const_cast<Solution*>(this))->swapNurse( w2, nurse, nurse2 );

                if (isValidPrior( nurse, assign[nurse][w2].shift, w2 )
                    && isValidPrior( nurse2, assign[nurse2][w2].shift, w2 )) {
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
                    if (noTabu[w][w2]) {
#endif
                        if (rs.isMinimal( delta, minDelta )) {
                            minDelta = delta;
                            weekday = w;
                            weekday2 = w2;
                            minNurseDelta = delta1;
                            minNurse2Delta = delta2;
                        }
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
                    } else {    // tabu
                        if (rs_tabu.isMinimal( delta, minDelta_tabu )) {
                            minDelta_tabu = delta;
                            weekday_tabu = w;
                            weekday2_tabu = w2;
                            minNurseDelta_tabu = delta1;
                            minNurse2Delta_tabu = delta2;
                        }
                    }
#endif
                }
            } else {    // two day off
                delta -= DefaultPenalty::FORBIDDEN_MOVE;
            }
        }

        if (w == w2) { continue; }  // the first day is not swapped

        do {
            delta += trySwapNurse( w, nurse, nurse2 );
            delta1 += nurseDelta;
            delta2 += nurse2Delta;
            if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                (const_cast<Solution*>(this))->swapNurse( w, nurse, nurse2 );
            } else {    // two day off
                (delta -= DefaultPenalty::FORBIDDEN_MOVE);
            }
            ++w;
        } while ((w < w2)
            && !(isValidSuccession( nurse, assign[nurse][w].shift, w )
            && isValidSuccession( nurse2, assign[nurse2][w].shift, w )));

        while (w < (w2--)) {
            if (isValidPrior( nurse, assign[nurse][w2].shift, w2 )
                && isValidPrior( nurse2, assign[nurse2][w2].shift, w2 )) {
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
                if (noTabu[w][w2]) {
#endif
                    if (rs.isMinimal( delta, minDelta )) {
                        minDelta = delta;
                        weekday = w;
                        weekday2 = w2;
                        minNurseDelta = delta1;
                        minNurse2Delta = delta2;
                    }
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
                } else {    // tabu
                    if (rs_tabu.isMinimal( delta, minDelta_tabu )) {
                        minDelta_tabu = delta;
                        weekday_tabu = w;
                        weekday2_tabu = w2;
                        minNurseDelta_tabu = delta1;
                        minNurse2Delta_tabu = delta2;
                    }
                }
#endif
            }

            delta += trySwapNurse( w2, nurse, nurse2 );
            delta1 += nurseDelta;
            delta2 += nurse2Delta;
            if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                (const_cast<Solution*>(this))->swapNurse( w2, nurse, nurse2 );
            } else {    // two day off
                delta -= DefaultPenalty::FORBIDDEN_MOVE;
            }
        }
    }

    nurseDelta = minNurseDelta;
    nurse2Delta = minNurse2Delta;
#if INRC2_BLOCK_SWAP_TABU_STRENGTH != INRC2_BLOCK_SWAP_NO_TABU
    if (aspirationCritiera( minDelta, minDelta_tabu )) {
        minDelta = minDelta_tabu;
        weekday = weekday_tabu;
        weekday2 = weekday2_tabu;
        nurseDelta = minNurseDelta_tabu;
        nurse2Delta = minNurse2Delta_tabu;
    }
#endif
    return minDelta;
}

NurseRostering::ObjValue NurseRostering::Solution::trySwapBlock_fast( const Move &move ) const
{
    penalty.setBlockSwapMode();
    ObjValue delta = trySwapBlock( move.weekday, move.weekday2, move.nurse, move.nurse2 );
    penalty.setDefaultMode();

    return delta;
}

NurseRostering::ObjValue NurseRostering::Solution::tryExchangeDay( int weekday, NurseID nurse, int weekday2 ) const
{
    // TODO : make sure they won't be the same and leave out this
    if (weekday == weekday2) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    // both are day off will change nothing
    if (!assign.isWorking( nurse, weekday ) && !assign.isWorking( nurse, weekday2 )) {
        return DefaultPenalty::FORBIDDEN_MOVE;
    }

    // check succession 
    ShiftID shift = assign[nurse][weekday].shift;
    ShiftID shift2 = assign[nurse][weekday2].shift;
    if (weekday == weekday2 + 1) {
        if (!(isValidSuccession( nurse, shift, weekday2 )
            && problem.scenario.shifts[shift].legalNextShifts[shift2]
            && isValidPrior( nurse, shift2, weekday ))) {
            return DefaultPenalty::FORBIDDEN_MOVE;
        }
    } else if (weekday == weekday2 - 1) {
        if (!(isValidSuccession( nurse, shift2, weekday )
            && problem.scenario.shifts[shift2].legalNextShifts[shift]
            && isValidPrior( nurse, shift, weekday2 ))) {
            return DefaultPenalty::FORBIDDEN_MOVE;
        }
    } else {
        if (!(isValidSuccession( nurse, shift, weekday2 )
            && isValidPrior( nurse, shift, weekday2 )
            && isValidSuccession( nurse, shift2, weekday )
            && isValidPrior( nurse, shift2, weekday ))) {
            return DefaultPenalty::FORBIDDEN_MOVE;
        }
    }

    ObjValue delta = 0;

    if (assign.isWorking( nurse, weekday )) {
        if (assign.isWorking( nurse, weekday2 )) {
            delta += tryChangeAssign( weekday, nurse, assign[nurse][weekday2] );
            if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                Assign a( assign[nurse][weekday] );
                (const_cast<Solution*>(this))->changeAssign( weekday, nurse, assign[nurse][weekday2] );
                delta += tryChangeAssign( weekday2, nurse, a );
                (const_cast<Solution*>(this))->changeAssign( weekday, nurse, a );
            }
        } else {
            delta += tryRemoveAssign( weekday, nurse );
            if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                Assign a( assign[nurse][weekday] );
                (const_cast<Solution*>(this))->removeAssign( weekday, nurse );
                delta += tryAddAssign( weekday2, nurse, a );
                (const_cast<Solution*>(this))->addAssign( weekday, nurse, a );
            }
        }
    } else {
        delta += tryAddAssign( weekday, nurse, assign[nurse][weekday2] );
        if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
            (const_cast<Solution*>(this))->addAssign( weekday, nurse, assign[nurse][weekday2] );
            delta += tryRemoveAssign( weekday2, nurse );
            (const_cast<Solution*>(this))->removeAssign( weekday, nurse );
        }
    }

    return delta;
}

NurseRostering::ObjValue NurseRostering::Solution::tryExchangeDay( const Move &move ) const
{
    penalty.setExchangeMode();
    ObjValue delta = tryExchangeDay( move.weekday, move.nurse, move.weekday2 );
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

void NurseRostering::Solution::exchangeDay( int weekday, NurseID nurse, int weekday2 )
{
    if (assign.isWorking( nurse, weekday )) {
        if (assign.isWorking( nurse, weekday2 )) {
            Assign a( assign[nurse][weekday] );
            changeAssign( weekday, nurse, assign[nurse][weekday2] );
            changeAssign( weekday2, nurse, a );
        } else {
            addAssign( weekday2, nurse, assign[nurse][weekday] );
            removeAssign( weekday, nurse );
        }
    } else {
        if (assign.isWorking( nurse, weekday2 )) {
            addAssign( weekday, nurse, assign[nurse][weekday2] );
            removeAssign( weekday2, nurse );
        }
    }
}

void NurseRostering::Solution::exchangeDay( const Move &move )
{
    exchangeDay( move.weekday, move.nurse, move.weekday2 );
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


void NurseRostering::Solution::updateDayTabu( NurseID nurse, int weekday )
{
    dayTabu[nurse][weekday] = iterCount +
        solver.DayTabuTenureBase() + (rand() % solver.DayTabuTenureAmp());
}

void NurseRostering::Solution::updateShiftTabu( NurseID nurse, int weekday, const Assign &a )
{
    shiftTabu[nurse][weekday][a.shift][a.skill] = iterCount +
        solver.ShiftTabuTenureBase() + (rand() % solver.ShiftTabuTenureAmp());
}


bool NurseRostering::Solution::checkIncrementalUpdate()
{
    bool correct = true;
    ostringstream oss;
    oss << iterCount;

    ObjValue incrementalVal = objValue;
    evaluateObjValue();
    if (solver.checkFeasibility( assign ) != 0) {
        solver.errorLog( "infeasible solution @" + oss.str() );
        correct = false;
    }
    ObjValue checkResult = solver.checkObjValue( assign );
    if (checkResult != objValue) {
        solver.errorLog( "check conflict with evaluate @" + oss.str() );
        correct = false;
    }
    if (objValue != incrementalVal) {
        solver.errorLog( "evaluate conflict with incremental update @" + oss.str() );
        correct = false;
    }

    return correct;
}

void NurseRostering::Solution::evaluateInsufficientStaff()
{
    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        for (ShiftID shift = NurseRostering::Scenario::Shift::ID_BEGIN;
            shift < problem.scenario.shiftSize; ++shift) {
            for (SkillID skill = NurseRostering::Scenario::Skill::ID_BEGIN;
                skill < problem.scenario.skillSize; ++skill) {
                if (missingNurseNums[weekday][shift][skill] > 0) {
                    objInsufficientStaff += penalty.InsufficientStaff() * missingNurseNums[weekday][shift][skill];
                }
            }
        }
    }
}

void NurseRostering::Solution::evaluateConsecutiveShift()
{
    const History &history( problem.history );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
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
                        objConsecutiveShift += penalty.ConsecutiveShift() * (c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1);
                    } else {
                        objConsecutiveShift += penalty.ConsecutiveShift() * distanceToRange(
                            c.shiftHigh[Weekday::Mon] - c.shiftLow[Weekday::Mon] + 1,
                            shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    }
                } else {
                    objConsecutiveShift += penalty.ConsecutiveShift() * distanceToRange(
                        c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1, shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    if (Assign::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += penalty.ConsecutiveShift() * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else if (Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveShift += penalty.ConsecutiveShift() * absentCount(
                    history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
            }
            // handle blocks in the middle of the week
            for (; c.shiftHigh[nextday] < Weekday::Sun; nextday = c.shiftHigh[nextday] + 1) {
                if (assign.isWorking( nurse, nextday )) {
                    const ShiftID &shiftID( assign[nurse][nextday].shift );
                    const Scenario::Shift &shift( shifts[shiftID] );
                    objConsecutiveShift += penalty.ConsecutiveShift() *
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
                        objConsecutiveShift += penalty.ConsecutiveShift() * Weekday::NUM;
                    } else {
                        objConsecutiveShift += penalty.ConsecutiveShift() * exceedCount(
                            consecutiveShift_EntireWeek, shift.maxConsecutiveShiftNum );
                    }
                } else {    // different shifts
                    if (Weekday::NUM > shift.maxConsecutiveShiftNum) {
                        objConsecutiveShift += penalty.ConsecutiveShift() *
                            (Weekday::NUM - shift.maxConsecutiveShiftNum);
                    }
                    if (Assign::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += penalty.ConsecutiveShift() * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else {
                objConsecutiveShift += penalty.ConsecutiveShift() * exceedCount(
                    consecutiveShift, shift.maxConsecutiveShiftNum );
            }
        } else if (c.isSingleConsecutiveShift() // the entire week is one block
            && Assign::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveShift += penalty.ConsecutiveShift() * absentCount(
                history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
        }
    }
}

void NurseRostering::Solution::evaluateConsecutiveDay()
{
    const History &history( problem.history );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const ContractID &contractID( problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( problem.scenario.contracts[contractID] );

        int nextday = c.dayHigh[Weekday::Mon] + 1;
        if (nextday <= Weekday::Sun) {   // the entire week is not one block
            // handle first block with history
            if (assign.isWorking( nurse, Weekday::Mon )) {
                if (history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                    // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                    objConsecutiveDay += penalty.ConsecutiveDay() * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDay += penalty.ConsecutiveDay() * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
            } else if (Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveDay += penalty.ConsecutiveDay() * absentCount
                    ( history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
            }
            // handle blocks in the middle of the week
            for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
                if (assign.isWorking( nurse, nextday )) {
                    objConsecutiveDay += penalty.ConsecutiveDay() *
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
                    objConsecutiveDay += penalty.ConsecutiveDay() * Weekday::NUM;
                } else {
                    objConsecutiveDay += penalty.ConsecutiveDay() * exceedCount(
                        consecutiveDay, contract.maxConsecutiveDayNum );
                }
            } else {
                objConsecutiveDay += penalty.ConsecutiveDay() * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayNum );
            }
        } else if (c.isSingleConsecutiveDay() // the entire week is one block
            && Assign::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveDay += penalty.ConsecutiveDay() * absentCount(
                history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
        }
    }
}

void NurseRostering::Solution::evaluateConsecutiveDayOff()
{
    const History &history( problem.history );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const ContractID &contractID( problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( problem.scenario.contracts[contractID] );

        int nextday = c.dayHigh[Weekday::Mon] + 1;
        if (nextday <= Weekday::Sun) {   // the entire week is not one block
            // handle first block with history
            if (!assign.isWorking( nurse, Weekday::Mon )) {
                if (history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                    objConsecutiveDayOff += penalty.ConsecutiveDayOff() * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDayOff += penalty.ConsecutiveDayOff() * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
            } else if (!Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveDayOff += penalty.ConsecutiveDayOff() * absentCount(
                    history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
            }
            // handle blocks in the middle of the week
            for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
                if (!assign.isWorking( nurse, nextday )) {
                    objConsecutiveDayOff += penalty.ConsecutiveDayOff() *
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
                    objConsecutiveDayOff += penalty.ConsecutiveDayOff() * Weekday::NUM;
                } else {
                    objConsecutiveDayOff += penalty.ConsecutiveDayOff() * exceedCount(
                        consecutiveDay, contract.maxConsecutiveDayoffNum );
                }
            } else {
                objConsecutiveDayOff += penalty.ConsecutiveDayOff() * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayoffNum );
            }
        } else if (c.isSingleConsecutiveDay() // the entire week is one block
            && (!Assign::isWorking( history.lastShifts[nurse] ))) {
            objConsecutiveDayOff += penalty.ConsecutiveDayOff() * absentCount(
                history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
        }
    }
}

void NurseRostering::Solution::evaluatePreference()
{
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
            objPreference += penalty.Preference() *
                problem.weekData.shiftOffs[weekday][shift][nurse];
        }
    }
}

void NurseRostering::Solution::evaluateCompleteWeekend()
{
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        objCompleteWeekend += penalty.CompleteWeekend() *
            (problem.scenario.contracts[problem.scenario.nurses[nurse].contract].completeWeekend
            && (assign.isWorking( nurse, Weekday::Sat ) != assign.isWorking( nurse, Weekday::Sun )));
    }
}

void NurseRostering::Solution::evaluateTotalAssign()
{
    const Scenario &scenario( problem.scenario );

    for (NurseID nurse = 0; nurse < scenario.nurseNum; ++nurse) {
        int min = scenario.nurses[nurse].restMinShiftNum;
        int max = scenario.nurses[nurse].restMaxShiftNum;
        objTotalAssign += penalty.TotalAssign() * distanceToRange(
            totalAssignNums[nurse] * problem.history.restWeekCount,
            min, max ) / problem.history.restWeekCount;
    }
}

void NurseRostering::Solution::evaluateTotalWorkingWeekend()
{
    int totalWeekNum = problem.scenario.totalWeekNum;

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        int maxWeekend = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
        int historyWeekend = problem.history.totalWorkingWeekendNums[nurse] * totalWeekNum;
        int exceedingWeekend = historyWeekend - (maxWeekend * problem.history.currentWeek) +
            ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * totalWeekNum);
        if (exceedingWeekend > 0) {
            objTotalWorkingWeekend += penalty.TotalWorkingWeekend() *
                exceedingWeekend / totalWeekNum;
        }
#ifdef INRC2_LOG
        // remove penalty in the history except the first week
        if (problem.history.pastWeekCount > 0) {
            historyWeekend -= maxWeekend * problem.history.pastWeekCount;
            if (historyWeekend > 0) {
                objTotalWorkingWeekend -= penalty.TotalWorkingWeekend() *
                    historyWeekend / totalWeekNum;
            }
        }
#endif
    }
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
            int n = rand() % validNurseNum_CurShift[minSkillNum];
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
