#include "TabuSolver.h"


using namespace std;


const string NurseRostering::TabuSolver::Name( "Tabu" );



NurseRostering::TabuSolver::TabuSolver( const NurseRostering &i, clock_t st )
    :Solver( i, Name, st ), sln( *this ),
    nurseNumOfSkill( vector<int>( i.scenario.skillTypeNum, 0 ) ),
    nurseWithSkill( vector< vector< vector<NurseID> > >( problem.scenario.skillTypeNum ) )
{
}

void NurseRostering::TabuSolver::init()
{
    srand( problem.randSeed );

    initAssistData();

    //sln.genInitAssign_BranchAndBound();
    if (sln.genInitAssign() == false) {
        Timer timer( REPAIR_TIMEOUT_IN_INIT, startTime );
        sln.repair( timer );
    }

    sln.evaluateObjValue();

    optima = sln.genOutput();
}

void NurseRostering::TabuSolver::solve()
{
    // TODO
    Timer timer( problem.timeout, startTime );
    sln.iterativeLocalSearch( timer, optima );
}

void NurseRostering::TabuSolver::initAssistData()
{
    for (NurseID n = 0; n < problem.scenario.nurseNum; ++n) {
        const vector<bool> &skills = problem.scenario.nurses[n].skills;
        unsigned skillNum = problem.scenario.nurses[n].skillNum;
        for (int skill = 0; skill < problem.scenario.skillTypeNum; ++skill) {
            if (skills[skill]) {
                ++nurseNumOfSkill[skill];
                if (skillNum > nurseWithSkill[skill].size()) {
                    nurseWithSkill[skill].resize( skillNum );
                }
                nurseWithSkill[skill][skillNum - 1].push_back( n );
            }
        }
    }
}



NurseRostering::TabuSolver::Solution::Solution( const TabuSolver &s )
    : solver( s )
{
    resetAssign();
}

NurseRostering::TabuSolver::Solution::Solution( const TabuSolver &s, const Assign &a )
    : solver( s )
{
    rebuildAssistData( a );
}

void NurseRostering::TabuSolver::Solution::rebuildAssistData( const Assign &a )
{
#ifdef INRC2_DEBUG
    if (&a == &assign) {
        cerr << "self assignment is not allowed!" << endl;
    }
#endif

    resetAssign();
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (Assign::isWorking( a[nurse][weekday].shift )) {
                addShift( weekday, nurse, a[nurse][weekday].shift, a[nurse][weekday].skill );
            }
        }
    }
}

bool NurseRostering::TabuSolver::Solution::genInitAssign()
{
    AvailableNurses availableNurse( *this );

    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
        // decide assign sequence of skill
        // the greater requiredNurseNum/nurseNumOfSkill[skill] is, the smaller index in skillRank a skill will get
        vector<SkillID> skillRank( solver.problem.scenario.skillTypeNum );
        vector<double> dailyRequire( solver.problem.scenario.skillTypeNum, 0 );
        for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
            skillRank[skill] = skill;
            for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                dailyRequire[skill] += solver.problem.weekData.minNurseNums[weekday][shift][skill];
            }
            dailyRequire[skill] /= solver.nurseNumOfSkill[skill];
        }

        class CmpDailyRequire
        {
        public:
            CmpDailyRequire( vector<double> &dr ) :dailyRequire( dr ) {}

            bool operator()( const SkillID &l, const SkillID &r )
            {
                return (dailyRequire[l] > dailyRequire[r]);
            }
        private:
            vector<double> &dailyRequire;

        private:    // forbidden operators
            CmpDailyRequire& operator=(const CmpDailyRequire &) { return *this; }
        }cmpDailyRequire( dailyRequire );
        sort( skillRank.begin(), skillRank.end(), cmpDailyRequire );

        // start assigning nurses
        for (int rank = 0; rank < solver.problem.scenario.skillTypeNum; ++rank) {
            SkillID skill = skillRank[rank];
            availableNurse.setEnvironment( weekday, skill );
            for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                availableNurse.setShift( shift );
                for (int i = 0; i < solver.problem.weekData.minNurseNums[weekday][shift][skill]; ++i) {
                    int nurse = availableNurse.getNurse();
                    if (nurse != NurseRostering::Scenario::Nurse::ID_NONE) {
                        addShift( weekday, nurse, shift, skill );
                    } else {
#ifdef INRC2_DEBUG
                        cerr << "fail to generate feasible solution." << endl;
#endif
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

bool NurseRostering::TabuSolver::Solution::genInitAssign_BranchAndCut()
{
    bool feasible = fillAssign( Weekday::Mon, 0, 0, 0, 0 );
    Assign a( assign );
    rebuildAssistData( a );
    return feasible;
}

bool NurseRostering::TabuSolver::Solution::fillAssign( int weekday, ShiftID shift, SkillID skill, NurseID nurse, int nurseNum )
{
    if (nurse >= solver.problem.scenario.nurseNum) {
        if (nurseNum < solver.problem.weekData.minNurseNums[weekday][shift][skill]) {
            return false;
        } else {
            return fillAssign( weekday, shift, skill + 1, 0, 0 );
        }
    } else if (skill >= solver.problem.scenario.skillTypeNum) {
        return fillAssign( weekday, shift + 1, 0, 0, 0 );
    } else if (shift >= solver.problem.scenario.shiftTypeNum) {
        return fillAssign( weekday + 1, 0, 0, 0, 0 );
    } else if (weekday > Weekday::Sun) {
        return true;
    }

    SingleAssign firstAssign( shift, skill );
    SingleAssign secondAssign;
    NurseID firstNurseNum = nurseNum + 1;
    NurseID secondNurseNum = nurseNum;
    bool isNotAssignedBefore = !assign.isWorking( nurse, weekday );

    if (isNotAssignedBefore) {
        if (solver.problem.scenario.nurses[nurse].skills[skill]
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
        assign[nurse][weekday] = SingleAssign();
    }

    return false;
}


void NurseRostering::TabuSolver::Solution::resetAssign()
{
    assign = Assign( solver.problem.scenario.nurseNum, Weekday::SIZE );
    missingNurseNums = solver.problem.weekData.optNurseNums;
    totalAssignNums = solver.problem.history.totalAssignNums;
    consecutives = vector<Consecutive>( solver.problem.scenario.nurseNum );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        consecutives[nurse] = Consecutive( solver.problem.history, nurse );
        assign[nurse][Weekday::HIS] = SingleAssign( solver.problem.history.lastShifts[nurse] );
    }
}

void NurseRostering::TabuSolver::Solution::evaluateObjValue()
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
    evaluateTotalAssign();
    evaluateTotalWorkingWeekend();

    objValue = objInsufficientStaff + objConsecutiveShift + objConsecutiveDay + objConsecutiveDayOff
        + objPreference + objCompleteWeekend + objTotalAssign + objTotalWorkingWeekend;
}

bool NurseRostering::TabuSolver::Solution::repair( const Timer &timer )
{
    // TODO
    bool feasible;
    do {
        resetAssign();
        feasible = genInitAssign();
    } while (!feasible && !timer.isTimeOut());

    return feasible;
}

void NurseRostering::TabuSolver::Solution::iterativeLocalSearch( const Timer &timer, Output &optima )
{
    const int timeForEachLoop = CLOCKS_PER_SEC * 1;
    int loopCount = timer.restTime() / timeForEachLoop;
    for (; !timer.isTimeOut() && loopCount > 0; --loopCount) {
        Timer t( timer.restTime() / loopCount, clock() );
        randomWalk( t, optima );
        if (rand() % 2) {
            localSearch( timer, optima );
        } else {
            localSearchOnConsecutiveBorder( timer, optima );
        }
    }
}

void NurseRostering::TabuSolver::Solution::localSearch( const Timer &timer, Output &optima )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
#endif

    long long iterCount = 0;
    int failCount = 0;

    while (!timer.isTimeOut() && (failCount < 3)) {
        Move bestMove;
        while (!timer.isTimeOut() && findBestAddShift( bestMove )) {
            addShift( bestMove.weekday, bestMove.nurse, bestMove.shift, bestMove.skill );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestChangeShift( bestMove )) {
            changeShift( bestMove.weekday, bestMove.nurse, bestMove.shift, bestMove.skill );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestRemoveShift( bestMove )) {
            removeShift( bestMove.weekday, bestMove.nurse );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cerr << "iter: " << iterCount << ' '
        << "time: " << duration << ' '
        << "speed: " << iterCount * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::TabuSolver::Solution::localSearchOnConsecutiveBorder( const Timer &timer, Output &optima )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
#endif

    long long iterCount = 0;
    int failCount = 0;

    while (!timer.isTimeOut() && (failCount < 3)) {
        Move bestMove;
        while (!timer.isTimeOut() && findBestAddShiftOnConsecutiveBorder( bestMove )) {
            addShift( bestMove.weekday, bestMove.nurse, bestMove.shift, bestMove.skill );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestChangeShiftOnConsecutiveBorder( bestMove )) {
            changeShift( bestMove.weekday, bestMove.nurse, bestMove.shift, bestMove.skill );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestRemoveShiftOnConsecutiveBorder( bestMove )) {
            removeShift( bestMove.weekday, bestMove.nurse );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cerr << "iter: " << iterCount << ' '
        << "time: " << duration << ' '
        << "speed: " << iterCount * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::TabuSolver::Solution::randomWalk( const Timer &timer, Output &optima )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
#endif

    long long iterCount = 0;

    ObjValue delta;
    for (; !timer.isTimeOut(); ++iterCount) {
        int select = rand() % 3;
        int weekday = (rand() % Weekday::NUM) + Weekday::Mon;
        NurseID nurse = rand() % solver.problem.scenario.nurseNum;
        ShiftID shift = rand() % solver.problem.scenario.shiftTypeNum;
        SkillID skill = rand() % solver.problem.scenario.skillTypeNum;
        switch (select) {
            case 0:
                delta = tryAddShift( weekday, nurse, shift, skill );
                if (delta < MAX_OBJ_VALUE) {
                    objValue += delta;
                    addShift( weekday, nurse, shift, skill );
                }
                break;
            case 1:
                delta = tryChangeShift( weekday, nurse, shift, skill );
                if (delta < MAX_OBJ_VALUE) {
                    objValue += delta;
                    changeShift( weekday, nurse, shift, skill );
                }
                break;
            case 2:
                delta = tryRemoveShift( weekday, nurse );
                if (delta < MAX_OBJ_VALUE) {
                    objValue += delta;
                    removeShift( weekday, nurse );
                }
                break;
        }

        if (objValue < optima.objVal) {
            optima = genOutput();
        }
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cerr << "iter: " << iterCount << ' '
        << "time: " << duration << ' '
        << "speed: " << iterCount * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

bool NurseRostering::TabuSolver::Solution::findBestAddShift( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = MAX_OBJ_VALUE;

    // search for best addShift
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (!assign.isWorking( nurse, weekday )) {
                for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                    for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
                        ObjValue delta = tryAddShift( weekday, nurse, shift, skill );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, shift, skill );
                        }
                    }
                }
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::TabuSolver::Solution::findBestChangeShift( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
                for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                    for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
                        ObjValue delta = tryChangeShift( weekday, nurse, shift, skill );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, shift, skill );
                        }
                    }
                }
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::TabuSolver::Solution::findBestRemoveShift( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
                ObjValue delta = tryRemoveShift( weekday, nurse );
                if (rs.isMinimal( delta, bestMove.delta )) {
                    bestMove = Move( delta, nurse, weekday );
                }
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::TabuSolver::Solution::findBestAddShiftOnConsecutiveBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = MAX_OBJ_VALUE;

    // search for best addShift
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE;) {
            if (!assign.isWorking( nurse, weekday )) {
                for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                    for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
                        ObjValue delta = tryAddShift( weekday, nurse, shift, skill );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, shift, skill );
                        }
                    }
                }
                weekday = (weekday != c.dayHigh[weekday]) ? c.dayHigh[weekday] : (weekday + 1);
            } else {
                weekday = c.dayHigh[weekday] + 1;
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::TabuSolver::Solution::findBestChangeShiftOnConsecutiveBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE;) {
            if (assign.isWorking( nurse, weekday )) {
                for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                    for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
                        ObjValue delta = tryChangeShift( weekday, nurse, shift, skill );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, shift, skill );
                        }
                    }
                }
                weekday = (weekday != c.shiftHigh[weekday]) ? c.shiftHigh[weekday] : (weekday + 1);
            } else {
                weekday = c.shiftHigh[weekday] + 1;
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::TabuSolver::Solution::findBestRemoveShiftOnConsecutiveBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE;) {
            if (assign.isWorking( nurse, weekday )) {
                ObjValue delta = tryRemoveShift( weekday, nurse );
                if (rs.isMinimal( delta, bestMove.delta )) {
                    bestMove = Move( delta, nurse, weekday );
                }
                weekday = (weekday != c.dayHigh[weekday]) ? c.dayHigh[weekday] : (weekday + 1);
            } else {
                weekday = c.dayHigh[weekday] + 1;
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::TabuSolver::Solution::isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const
{
    return (!assign.isWorking( nurse, weekday - 1 )
        || solver.problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[shift]);
}

bool NurseRostering::TabuSolver::Solution::isValidPrior( NurseID nurse, ShiftID shift, int weekday ) const
{
    return ((weekday >= Weekday::Sun) || (!assign.isWorking( nurse, weekday + 1 ))
        || solver.problem.scenario.shifts[shift].legalNextShifts[assign[nurse][weekday + 1].shift]);
}

NurseRostering::ObjValue NurseRostering::TabuSolver::Solution::tryAddShift( int weekday, NurseID nurse, ShiftID shiftID, SkillID skillID ) const
{
    ShiftID oldShiftID = assign[nurse][weekday].shift;
    // TODO : make sure they won't be the same and leave out this
    if (!Assign::isWorking( shiftID ) || (shiftID == oldShiftID)
        || Assign::isWorking( oldShiftID )) {
        return MAX_OBJ_VALUE;
    }

    if (!solver.problem.scenario.nurses[nurse].skills[skillID]) {
        return MAX_OBJ_VALUE;
    }

    if (!(isValidSuccession( nurse, shiftID, weekday )
        && isValidPrior( nurse, shiftID, weekday ))) {
        return MAX_OBJ_VALUE;
    }

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    ObjValue delta = 0;
    ContractID contractID = solver.problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;
    int currentWeek = solver.problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta -= Penalty::InsufficientStaff *
        (missingNurseNums[weekday][shiftID][skillID] > 0);

    // consecutive shift
    const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );
    const Scenario::Shift &shift( shifts[shiftID] );
    ShiftID prevShiftID = assign[nurse][prevDay].shift;
    if (weekday == Weekday::Sun) {  // there is no blocks on the right
        // shiftHigh[weekday] will always be equal to Weekday::Sun
        if ((Weekday::Sun == c.shiftLow[weekday])
            && (shiftID == prevShiftID)) {
            const Scenario::Shift &prevShift( shifts[prevShiftID] );
            delta -= Penalty::ConsecutiveShift *
                distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sat],
                prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
            delta += Penalty::ConsecutiveShift * exceedCount(
                Weekday::Sun - c.shiftLow[Weekday::Sat] + 1, shift.maxConsecutiveShiftNum );
        } else {    // have nothing to do with previous block
            delta += Penalty::ConsecutiveShift *    // penalty on day off is counted later
                exceedCount( 1, shift.maxConsecutiveShiftNum );
        }
    } else {
        ShiftID nextShiftID = assign[nurse][nextDay].shift;
        if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
            int high = weekday;
            int low = weekday;
            if (prevShiftID == shiftID) {
                const Scenario::Shift &prevShift( shifts[prevShiftID] );
                low = c.shiftLow[prevDay];
                delta -= Penalty::ConsecutiveShift *
                    distanceToRange( weekday - c.shiftLow[prevDay],
                    prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
            }
            if (nextShiftID == shiftID) {
                const Scenario::Shift &nextShift( shifts[nextShiftID] );
                high = c.shiftHigh[nextDay];
                delta -= Penalty::ConsecutiveShift * penaltyDayNum(
                    c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                    nextShift.minConsecutiveShiftNum, nextShift.maxConsecutiveShiftNum );
            }
            delta += Penalty::ConsecutiveShift * penaltyDayNum( high - low + 1, high,
                shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
        } else if (weekday == c.shiftHigh[weekday]) {
            if (shiftID == nextShiftID) {
                const Scenario::Shift &nextShift( shifts[nextShiftID] );
                int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                if (consecutiveShiftOfNextBlock >= nextShift.maxConsecutiveShiftNum) {
                    delta += Penalty::ConsecutiveShift;
                } else if ((c.shiftHigh[nextDay] < Weekday::Sun)
                    && (consecutiveShiftOfNextBlock < nextShift.minConsecutiveShiftNum)) {
                    delta -= Penalty::ConsecutiveShift;
                }
            } else {
                delta += Penalty::ConsecutiveShift * distanceToRange( 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            }
        } else if (weekday == c.shiftLow[weekday]) {
            if (shiftID == prevShiftID) {
                const Scenario::Shift &prevShift( shifts[prevShiftID] );
                int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                if (consecutiveShiftOfPrevBlock >= prevShift.maxConsecutiveShiftNum) {
                    delta += Penalty::ConsecutiveShift;
                } else if (consecutiveShiftOfPrevBlock < prevShift.minConsecutiveShiftNum) {
                    delta -= Penalty::ConsecutiveShift;
                }
            } else {
                delta += Penalty::ConsecutiveShift * distanceToRange( 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            }
        } else {
            delta += Penalty::ConsecutiveShift * distanceToRange( 1,
                shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
        }
    }

    // consecutive day and day-off
    if (weekday == Weekday::Sun) {  // there is no block on the right
        // dayHigh[weekday] will always be equal to Weekday::Sun
        if (Weekday::Sun == c.dayLow[weekday]) {
            delta -= Penalty::ConsecutiveDay *
                distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= Penalty::ConsecutiveDayOff *
                exceedCount( 1, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sat] + 1, contract.maxConsecutiveDayNum );
        } else {    // day off block length over 1
            delta -= Penalty::ConsecutiveDayOff * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sun] + 1, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDayOff * distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sun],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay * exceedCount( 1, contract.maxConsecutiveDayNum );
        }
    } else {
        if (c.dayHigh[weekday] == c.dayLow[weekday]) {
            delta -= Penalty::ConsecutiveDay * distanceToRange( weekday - c.dayLow[prevDay],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= Penalty::ConsecutiveDayOff * distanceToRange( 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= Penalty::ConsecutiveDay * penaltyDayNum(
                c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDay * penaltyDayNum(
                c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        } else if (weekday == c.dayHigh[weekday]) {
            int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
            if (consecutiveDayOfNextBlock >= contract.maxConsecutiveDayNum) {
                delta += Penalty::ConsecutiveDay;
            } else if ((c.dayHigh[nextDay] < Weekday::Sun)
                && (consecutiveDayOfNextBlock < contract.minConsecutiveDayNum)) {
                delta -= Penalty::ConsecutiveDay;
            }
            int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            }
        } else if (weekday == c.dayLow[weekday]) {
            int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
            if (consecutiveDayOfPrevBlock >= contract.maxConsecutiveDayNum) {
                delta += Penalty::ConsecutiveDay;
            } else if (consecutiveDayOfPrevBlock < contract.minConsecutiveDayNum) {
                delta -= Penalty::ConsecutiveDay;
            }
            int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if ((c.dayHigh[weekday] < Weekday::Sun)
                && (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum)) {
                delta += Penalty::ConsecutiveDayOff;
            }
        } else {
            delta -= Penalty::ConsecutiveDayOff * penaltyDayNum(
                c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDayOff *
                distanceToRange( weekday - c.dayLow[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay * distanceToRange( 1,
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDayOff * penaltyDayNum(
                c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
        }
    }

    // preference
    const WeekData &weekData( solver.problem.weekData );
    delta += Penalty::Preference *
        weekData.shiftOffs[weekday][shiftID][nurse];

    if (weekday > Weekday::Fri) {
        int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
        // complete weekend
        if (contract.completeWeekend) {
            if (assign.isWorking( nurse, theOtherDay )) {
                delta -= Penalty::CompleteWeekend;
            } else {
                delta += Penalty::CompleteWeekend;
            }
        }

        // total working weekend
        if (!assign.isWorking( nurse, theOtherDay )) {
            const History &history( solver.problem.history );
            delta -= Penalty::TotalWorkingWeekend * exceedCount(
                history.totalWorkingWeekendNums[nurse] * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
            delta += Penalty::TotalWorkingWeekend * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
        }
    }

    // total assign
    delta -= Penalty::TotalAssign * distanceToRange(
        totalAssignNums[nurse] * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;
    delta += Penalty::TotalAssign * distanceToRange(
        (totalAssignNums[nurse] + 1) * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;

    return delta;
}

NurseRostering::ObjValue NurseRostering::TabuSolver::Solution::tryChangeShift( int weekday, NurseID nurse, ShiftID shiftID, SkillID skillID ) const
{
    ShiftID oldShiftID = assign[nurse][weekday].shift;
    SkillID oldSkillID = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if (!Assign::isWorking( shiftID ) || !Assign::isWorking( oldShiftID )
        || ((shiftID == oldShiftID) && (skillID == oldSkillID))) {
        return MAX_OBJ_VALUE;
    }

    if (!solver.problem.scenario.nurses[nurse].skills[skillID]) {
        return MAX_OBJ_VALUE;
    }

    if (!(isValidSuccession( nurse, shiftID, weekday )
        && isValidPrior( nurse, shiftID, weekday ))) {
        return MAX_OBJ_VALUE;
    }

    const WeekData &weekData( solver.problem.weekData );
    if ((weekData.optNurseNums[weekday][oldShiftID][oldSkillID] -
        missingNurseNums[weekday][oldShiftID][oldSkillID])
        <= weekData.minNurseNums[weekday][oldShiftID][oldSkillID]) {
        return MAX_OBJ_VALUE;
    }

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    ObjValue delta = 0;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta += Penalty::InsufficientStaff *
        (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);
    delta -= Penalty::InsufficientStaff *
        (missingNurseNums[weekday][shiftID][skillID] > 0);

    if (shiftID != oldShiftID) {
        // consecutive shift
        const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );
        const Scenario::Shift &shift( shifts[shiftID] );
        const Scenario::Shift &oldShift( solver.problem.scenario.shifts[oldShiftID] );
        ShiftID prevShiftID = assign[nurse][prevDay].shift;
        if (weekday == Weekday::Sun) {  // there is no blocks on the right
            // shiftHigh[weekday] will always equal to Weekday::Sun
            if (Weekday::Sun == c.shiftLow[weekday]) {
                if (shiftID == prevShiftID) {
                    const Scenario::Shift &prevShift( shifts[prevShiftID] );
                    delta -= Penalty::ConsecutiveShift *
                        distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sat],
                        prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                    delta -= Penalty::ConsecutiveShift *
                        exceedCount( 1, oldShift.maxConsecutiveShiftNum );
                    delta += Penalty::ConsecutiveShift * exceedCount(
                        Weekday::Sun - c.shiftLow[Weekday::Sat] + 1, shift.maxConsecutiveShiftNum );
                } else {
                    delta -= Penalty::ConsecutiveShift *
                        exceedCount( 1, oldShift.maxConsecutiveShiftNum );
                    delta += Penalty::ConsecutiveShift *
                        exceedCount( 1, shift.maxConsecutiveShiftNum );
                }
            } else {    // block length over 1
                delta -= Penalty::ConsecutiveShift * exceedCount(
                    Weekday::Sun - c.shiftLow[Weekday::Sun] + 1, oldShift.maxConsecutiveShiftNum );
                delta += Penalty::ConsecutiveShift * distanceToRange( Weekday::Sun - c.shiftLow[Weekday::Sun],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += Penalty::ConsecutiveShift *
                    exceedCount( 1, shift.maxConsecutiveShiftNum );
            }
        } else {
            ShiftID nextShiftID = assign[nurse][nextDay].shift;
            if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
                int high = weekday;
                int low = weekday;
                if (prevShiftID == shiftID) {
                    const Scenario::Shift &prevShift( shifts[prevShiftID] );
                    low = c.shiftLow[prevDay];
                    delta -= Penalty::ConsecutiveShift *
                        distanceToRange( weekday - c.shiftLow[prevDay],
                        prevShift.minConsecutiveShiftNum, prevShift.maxConsecutiveShiftNum );
                }
                if (nextShiftID == shiftID) {
                    const Scenario::Shift &nextShift( shifts[nextShiftID] );
                    high = c.shiftHigh[nextDay];
                    delta -= Penalty::ConsecutiveShift * penaltyDayNum(
                        c.shiftHigh[nextDay] - weekday, c.shiftHigh[nextDay],
                        nextShift.minConsecutiveShiftNum, nextShift.maxConsecutiveShiftNum );
                }
                delta -= Penalty::ConsecutiveShift * distanceToRange( 1,
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += Penalty::ConsecutiveShift * penaltyDayNum( high - low + 1, high,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
            } else if (weekday == c.shiftHigh[weekday]) {
                if (nextShiftID == shiftID) {
                    const Scenario::Shift &nextShift( shifts[nextShiftID] );
                    int consecutiveShiftOfNextBlock = c.shiftHigh[nextDay] - weekday;
                    if (consecutiveShiftOfNextBlock >= nextShift.maxConsecutiveShiftNum) {
                        delta += Penalty::ConsecutiveShift;
                    } else if ((c.shiftHigh[nextDay] < Weekday::Sun)
                        && (consecutiveShiftOfNextBlock < nextShift.minConsecutiveShiftNum)) {
                        delta -= Penalty::ConsecutiveShift;
                    }
                } else {
                    delta += Penalty::ConsecutiveShift * distanceToRange( 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
                int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
                if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                    delta -= Penalty::ConsecutiveShift;
                } else if (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum) {
                    delta += Penalty::ConsecutiveShift;
                }
            } else if (weekday == c.shiftLow[weekday]) {
                if (prevShiftID == shiftID) {
                    const Scenario::Shift &prevShift( shifts[prevShiftID] );
                    int consecutiveShiftOfPrevBlock = weekday - c.shiftLow[prevDay];
                    if (consecutiveShiftOfPrevBlock >= prevShift.maxConsecutiveShiftNum) {
                        delta += Penalty::ConsecutiveShift;
                    } else if (consecutiveShiftOfPrevBlock < prevShift.minConsecutiveShiftNum) {
                        delta -= Penalty::ConsecutiveShift;
                    }
                } else {
                    delta += Penalty::ConsecutiveShift * distanceToRange( 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
                int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
                if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                    delta -= Penalty::ConsecutiveShift;
                } else if ((c.shiftHigh[weekday] < Weekday::Sun)
                    && (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum)) {
                    delta += Penalty::ConsecutiveShift;
                }
            } else {
                delta -= Penalty::ConsecutiveShift * penaltyDayNum(
                    c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += Penalty::ConsecutiveShift *
                    distanceToRange( weekday - c.shiftLow[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
                delta += Penalty::ConsecutiveShift * distanceToRange( 1,
                    shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                delta += Penalty::ConsecutiveShift *
                    penaltyDayNum( c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                    oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            }
        }

        // preference
        delta += Penalty::Preference *
            weekData.shiftOffs[weekday][shiftID][nurse];
        delta -= Penalty::Preference *
            weekData.shiftOffs[weekday][oldShiftID][nurse];
    }

    return delta;
}

NurseRostering::ObjValue NurseRostering::TabuSolver::Solution::tryRemoveShift( int weekday, NurseID nurse ) const
{
    ShiftID oldShiftID = assign[nurse][weekday].shift;
    SkillID oldSkillID = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if (!Assign::isWorking( oldShiftID )) {
        return MAX_OBJ_VALUE;
    }

    const WeekData &weekData( solver.problem.weekData );
    if (((weekData.optNurseNums[weekday][oldShiftID][oldSkillID] -
        missingNurseNums[weekday][oldShiftID][oldSkillID])
        <= weekData.minNurseNums[weekday][oldShiftID][oldSkillID])) {
        return MAX_OBJ_VALUE;
    }

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    ObjValue delta = 0;
    ContractID contractID = solver.problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;
    int currentWeek = solver.problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta += Penalty::InsufficientStaff *
        (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);

    // consecutive shift
    const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );
    const Scenario::Shift &oldShift( shifts[oldShiftID] );
    if (weekday == Weekday::Sun) {  // there is no block on the right
        if (Weekday::Sun == c.shiftLow[weekday]) {
            delta -= Penalty::ConsecutiveShift * exceedCount( 1, oldShift.maxConsecutiveShiftNum );
        } else {
            delta -= Penalty::ConsecutiveShift * exceedCount(
                Weekday::Sun - c.shiftLow[weekday] + 1, oldShift.maxConsecutiveShiftNum );
            delta += Penalty::ConsecutiveShift * distanceToRange( Weekday::Sun - c.shiftLow[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
        }
    } else {
        if (c.shiftHigh[weekday] == c.shiftLow[weekday]) {
            delta -= Penalty::ConsecutiveShift * distanceToRange( 1,
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
        } else if (weekday == c.shiftHigh[weekday]) {
            int consecutiveShiftOfThisBlock = weekday - c.shiftLow[weekday] + 1;
            if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                delta -= Penalty::ConsecutiveShift;
            } else if (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum) {
                delta += Penalty::ConsecutiveShift;
            }
        } else if (weekday == c.shiftLow[weekday]) {
            int consecutiveShiftOfThisBlock = c.shiftHigh[weekday] - weekday + 1;
            if (consecutiveShiftOfThisBlock > oldShift.maxConsecutiveShiftNum) {
                delta -= Penalty::ConsecutiveShift;
            } else if ((c.shiftHigh[weekday] < Weekday::Sun)
                && (consecutiveShiftOfThisBlock <= oldShift.minConsecutiveShiftNum)) {
                delta += Penalty::ConsecutiveShift;
            }
        } else {
            delta -= Penalty::ConsecutiveShift * penaltyDayNum(
                c.shiftHigh[weekday] - c.shiftLow[weekday] + 1, c.shiftHigh[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            delta += Penalty::ConsecutiveShift * distanceToRange( weekday - c.shiftLow[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
            delta += Penalty::ConsecutiveShift * penaltyDayNum(
                c.shiftHigh[weekday] - weekday, c.shiftHigh[weekday],
                oldShift.minConsecutiveShiftNum, oldShift.maxConsecutiveShiftNum );
        }
    }

    // consecutive day and day-off
    if (weekday == Weekday::Sun) {  // there is no blocks on the right
        // dayHigh[weekday] will always be equal to Weekday::Sun
        if (Weekday::Sun == c.dayLow[weekday]) {
            delta -= Penalty::ConsecutiveDayOff *
                distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= Penalty::ConsecutiveDay *
                exceedCount( 1, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDayOff * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sat] + 1, contract.maxConsecutiveDayoffNum );
        } else {    // day off block length over 1
            delta -= Penalty::ConsecutiveDay * exceedCount(
                Weekday::Sun - c.dayLow[Weekday::Sun] + 1, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDay * distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sun],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDayOff * exceedCount( 1, contract.maxConsecutiveDayoffNum );
        }
    } else {
        if (c.dayHigh[weekday] == c.dayLow[weekday]) {
            delta -= Penalty::ConsecutiveDayOff *
                distanceToRange( weekday - c.dayLow[prevDay],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= Penalty::ConsecutiveDay *
                distanceToRange( 1,
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= Penalty::ConsecutiveDayOff * penaltyDayNum(
                c.dayHigh[nextDay] - weekday, c.dayHigh[nextDay],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDayOff * penaltyDayNum(
                c.dayHigh[nextDay] - c.dayLow[prevDay] + 1, c.dayHigh[nextDay],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
        } else if (weekday == c.dayHigh[weekday]) {
            int consecutiveDayOfNextBlock = c.dayHigh[nextDay] - weekday;
            if (consecutiveDayOfNextBlock >= contract.maxConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            } else if ((c.dayHigh[nextDay] < Weekday::Sun)
                && (consecutiveDayOfNextBlock < contract.minConsecutiveDayoffNum)) {
                delta -= Penalty::ConsecutiveDayOff;
            }
            int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayNum) {
                delta -= Penalty::ConsecutiveDay;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayNum) {
                delta += Penalty::ConsecutiveDay;
            }
        } else if (weekday == c.dayLow[weekday]) {
            int consecutiveDayOfPrevBlock = weekday - c.dayLow[prevDay];
            if (consecutiveDayOfPrevBlock >= contract.maxConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfPrevBlock < contract.minConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            }
            int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayNum) {
                delta -= Penalty::ConsecutiveDay;
            } else if ((c.dayHigh[weekday] < Weekday::Sun)
                && (consecutiveDayOfThisBlock <= contract.minConsecutiveDayNum)) {
                delta += Penalty::ConsecutiveDay;
            }
        } else {
            delta -= Penalty::ConsecutiveDay * penaltyDayNum(
                c.dayHigh[weekday] - c.dayLow[weekday] + 1, c.dayHigh[weekday],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDay *
                distanceToRange( weekday - c.dayLow[weekday],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDayOff * distanceToRange( 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay *
                penaltyDayNum( c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        }
    }

    // preference
    delta -= Penalty::Preference *
        weekData.shiftOffs[weekday][oldShiftID][nurse];

    if (weekday > Weekday::Fri) {
        int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
        // complete weekend
        if (contract.completeWeekend) {
            if (assign.isWorking( nurse, theOtherDay )) {
                delta += Penalty::CompleteWeekend;
            } else {
                delta -= Penalty::CompleteWeekend;
            }
        }

        // total working weekend
        if (!assign.isWorking( nurse, theOtherDay )) {
            const History &history( solver.problem.history );
            delta -= Penalty::TotalWorkingWeekend * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
            delta += Penalty::TotalWorkingWeekend * exceedCount(
                history.totalWorkingWeekendNums[nurse] * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
        }
    }

    // total assign
    delta -= Penalty::TotalAssign * distanceToRange(
        totalAssignNums[nurse] * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;
    delta += Penalty::TotalAssign * distanceToRange(
        (totalAssignNums[nurse] - 1) * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;

    return delta;
}

void NurseRostering::TabuSolver::Solution::addShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill )
{
    updateConsecutive( weekday, nurse, shift );

    --missingNurseNums[weekday][shift][skill];

    ++totalAssignNums[nurse];

    assign[nurse][weekday] = SingleAssign( shift, skill );
}

void NurseRostering::TabuSolver::Solution::changeShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill )
{
    if (shift != assign[nurse][weekday].shift) {
        updateConsecutive( weekday, nurse, shift );
    }

    --missingNurseNums[weekday][shift][skill];
    ++missingNurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];

    assign[nurse][weekday] = SingleAssign( shift, skill );
}

void NurseRostering::TabuSolver::Solution::removeShift( int weekday, NurseID nurse )
{
    updateConsecutive( weekday, nurse, NurseRostering::Scenario::Shift::ID_NONE );

    ++missingNurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];

    --totalAssignNums[nurse];

    assign[nurse][weekday] = SingleAssign();
}

void NurseRostering::TabuSolver::Solution::updateConsecutive( int weekday, NurseID nurse, ShiftID shift )
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

void NurseRostering::TabuSolver::Solution::assignHigh( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight )
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

void NurseRostering::TabuSolver::Solution::assignLow( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectLeft )
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

void NurseRostering::TabuSolver::Solution::assignMiddle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE] )
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

void NurseRostering::TabuSolver::Solution::assignSingle( int weekday, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight, bool affectLeft )
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

bool NurseRostering::TabuSolver::Solution::checkIncrementalUpdate()
{
    ObjValue incrementalVal = objValue;
    evaluateObjValue();
    if (!solver.checkFeasibility( assign )) {
        cerr << "infeasible solution." << endl;
        return false;
    }
    ObjValue checkResult = solver.checkObjValue( assign );
    if (checkResult != objValue) {
        cerr << "check conflict with evaluate." << endl;
        return false;
    }
    if (objValue != incrementalVal) {
        cerr << "evaluate conflict with incremental update." << endl;
        return false;
    }

    return true;
}

void NurseRostering::TabuSolver::Solution::evaluateInsufficientStaff()
{
    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
        for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
                if (missingNurseNums[weekday][shift][skill] > 0) {
                    objInsufficientStaff += Penalty::InsufficientStaff * missingNurseNums[weekday][shift][skill];
                }
            }
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateConsecutiveShift()
{
    const History &history( solver.problem.history );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );

        int nextday = c.shiftHigh[Weekday::Mon] + 1;
        if (nextday < Weekday::SIZE) {   // the entire week is not one block
            // handle first block with history
            if (assign.isWorking( nurse, Weekday::Mon )) {
                const Scenario::Shift &shift( shifts[assign[nurse][Weekday::Mon].shift] );
                if (history.lastShifts[nurse] == assign[nurse][Weekday::Mon].shift) {
                    if (history.consecutiveShiftNums[nurse] > shift.maxConsecutiveShiftNum) {
                        // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                        objConsecutiveShift += Penalty::ConsecutiveShift * (c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1);
                    } else {
                        objConsecutiveShift += Penalty::ConsecutiveShift * distanceToRange(
                            c.shiftHigh[Weekday::Mon] - c.shiftLow[Weekday::Mon] + 1,
                            shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    }
                } else {
                    objConsecutiveShift += Penalty::ConsecutiveShift * distanceToRange(
                        c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1, shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    if (Assign::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else if (Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                    history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
            }
            // handle blocks in the middle of the week
            for (; c.shiftHigh[nextday] < Weekday::Sun; nextday = c.shiftHigh[nextday] + 1) {
                if (assign.isWorking( nurse, nextday )) {
                    const ShiftID &shiftID( assign[nurse][nextday].shift );
                    const Scenario::Shift &shift( shifts[shiftID] );
                    objConsecutiveShift += Penalty::ConsecutiveShift *
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
            const Scenario::Shift &shift( solver.problem.scenario.shifts[shiftID] );
            if (c.isSingleConsecutiveShift()) { // the entire week is one block
                if (history.lastShifts[nurse] == assign[nurse][Weekday::Sun].shift) {
                    if (history.consecutiveShiftNums[nurse] > shift.maxConsecutiveShiftNum) {
                        objConsecutiveShift += Penalty::ConsecutiveShift * Weekday::NUM;
                    } else {
                        objConsecutiveShift += Penalty::ConsecutiveShift * exceedCount(
                            consecutiveShift_EntireWeek, shift.maxConsecutiveShiftNum );
                    }
                } else {    // different shifts
                    if (Weekday::NUM > shift.maxConsecutiveShiftNum) {
                        objConsecutiveShift += Penalty::ConsecutiveShift *
                            (Weekday::NUM - shift.maxConsecutiveShiftNum);
                    }
                    if (Assign::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else {
                objConsecutiveShift += Penalty::ConsecutiveShift * exceedCount(
                    consecutiveShift, shift.maxConsecutiveShiftNum );
            }
        } else if (c.isSingleConsecutiveShift() // the entire week is one block
            && Assign::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateConsecutiveDay()
{
    const History &history( solver.problem.history );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const ContractID &contractID( solver.problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );

        int nextday = c.dayHigh[Weekday::Mon] + 1;
        if (nextday < Weekday::SIZE) {   // the entire week is not one block
            // handle first block with history
            if (assign.isWorking( nurse, Weekday::Mon )) {
                if (history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                    // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                    objConsecutiveDay += Penalty::ConsecutiveDay * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDay += Penalty::ConsecutiveDay * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
            } else if (Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveDay += Penalty::ConsecutiveDay * absentCount
                    ( history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
            }
            // handle blocks in the middle of the week
            for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
                if (assign.isWorking( nurse, nextday )) {
                    objConsecutiveDay += Penalty::ConsecutiveDay *
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
                    objConsecutiveDay += Penalty::ConsecutiveDay * Weekday::NUM;
                } else {
                    objConsecutiveDay += Penalty::ConsecutiveDay * exceedCount(
                        consecutiveDay, contract.maxConsecutiveDayNum );
                }
            } else {
                objConsecutiveDay += Penalty::ConsecutiveDay * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayNum );
            }
        } else if (c.isSingleConsecutiveDay() // the entire week is one block
            && Assign::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveDay += Penalty::ConsecutiveDay * absentCount(
                history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateConsecutiveDayOff()
{
    const History &history( solver.problem.history );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const ContractID &contractID( solver.problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );

        int nextday = c.dayHigh[Weekday::Mon] + 1;
        if (nextday < Weekday::SIZE) {   // the entire week is not one block
            // handle first block with history
            if (!assign.isWorking( nurse, Weekday::Mon )) {
                if (history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
            } else if (!Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveDayOff += Penalty::ConsecutiveDayOff * absentCount(
                    history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
            }
            // handle blocks in the middle of the week
            for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
                if (!assign.isWorking( nurse, nextday )) {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff *
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
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * Weekday::NUM;
                } else {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * exceedCount(
                        consecutiveDay, contract.maxConsecutiveDayoffNum );
                }
            } else {
                objConsecutiveDayOff += Penalty::ConsecutiveDayOff * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayoffNum );
            }
        } else if (c.isSingleConsecutiveDay() // the entire week is one block
            && (!Assign::isWorking( history.lastShifts[nurse] ))) {
            objConsecutiveDayOff += Penalty::ConsecutiveDayOff * absentCount(
                history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluatePreference()
{
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
            if (Assign::isWorking( shift )) {
                objPreference += Penalty::Preference *
                    solver.problem.weekData.shiftOffs[weekday][shift][nurse];
            }
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateCompleteWeekend()
{
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        objCompleteWeekend += Penalty::CompleteWeekend *
            (solver.problem.scenario.contracts[solver.problem.scenario.nurses[nurse].contract].completeWeekend
            && (assign.isWorking( nurse, Weekday::Sat )
            != assign.isWorking( nurse, Weekday::Sun )));
    }
}

void NurseRostering::TabuSolver::Solution::evaluateTotalAssign()
{
    const Scenario &scenario( solver.problem.scenario );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;

    for (NurseID nurse = 0; nurse < scenario.nurseNum; ++nurse) {
        int min = scenario.contracts[scenario.nurses[nurse].contract].minShiftNum;
        int max = scenario.contracts[scenario.nurses[nurse].contract].maxShiftNum;
        objTotalAssign += Penalty::TotalAssign * distanceToRange(
            totalAssignNums[nurse] * totalWeekNum,
            min * solver.problem.history.currentWeek,
            max * solver.problem.history.currentWeek ) / totalWeekNum;
        // remove penalty in the history except the first week
        if (solver.problem.history.pastWeekCount > 0) {
            objTotalAssign -= Penalty::TotalAssign * distanceToRange(
                solver.problem.history.totalAssignNums[nurse] * totalWeekNum,
                min * solver.problem.history.pastWeekCount,
                max * solver.problem.history.pastWeekCount ) / totalWeekNum;
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateTotalWorkingWeekend()
{
    int totalWeekNum = solver.problem.scenario.totalWeekNum;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        int maxWeekend = solver.problem.scenario.contracts[solver.problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
        int historyWeekend = solver.problem.history.totalWorkingWeekendNums[nurse] * totalWeekNum;
        int exceedingWeekend = historyWeekend - (maxWeekend * solver.problem.history.currentWeek) +
            ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * totalWeekNum);
        if (exceedingWeekend > 0) {
            objTotalWorkingWeekend += Penalty::TotalWorkingWeekend *
                exceedingWeekend / totalWeekNum;
        }
        // remove penalty in the history except the first week
        if (solver.problem.history.pastWeekCount > 0) {
            historyWeekend -= maxWeekend * solver.problem.history.pastWeekCount;
            if (historyWeekend > 0) {
                objTotalWorkingWeekend -= Penalty::TotalWorkingWeekend *
                    historyWeekend / totalWeekNum;
            }
        }
    }
}

NurseRostering::History NurseRostering::TabuSolver::Solution::genHistory() const
{
    const History &history( solver.problem.history );
    History newHistory;
    newHistory.lastShifts.resize( solver.problem.scenario.nurseNum );
    newHistory.consecutiveShiftNums.resize( solver.problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayNums.resize( solver.problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayoffNums.resize( solver.problem.scenario.nurseNum, 0 );

    newHistory.pastWeekCount = history.currentWeek;
    newHistory.currentWeek = history.currentWeek + 1;
    newHistory.totalAssignNums = totalAssignNums;
    newHistory.totalWorkingWeekendNums = history.totalWorkingWeekendNums;
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        newHistory.totalWorkingWeekendNums[nurse] +=
            (assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun ));
        newHistory.lastShifts[nurse] = assign[nurse][Weekday::Sun].shift;
        const Consecutive &c( consecutives[nurse] );
        if (assign.isWorking( nurse, Weekday::Sun )) {
            newHistory.consecutiveShiftNums[nurse] =
                c.shiftHigh[Weekday::Sun] - c.shiftLow[Weekday::Sun] + 1;
            if (c.isSingleConsecutiveShift()
                && (history.lastShifts[nurse] == assign[nurse][Weekday::Sun].shift)) {
                newHistory.consecutiveShiftNums[nurse] += history.consecutiveShiftNums[nurse];
            }
            newHistory.consecutiveDayNums[nurse] =
                c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
            if (c.isSingleConsecutiveDay()) {
                newHistory.consecutiveDayNums[nurse] += history.consecutiveDayNums[nurse];
            }
        } else {
            newHistory.consecutiveDayoffNums[nurse] =
                c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
            if (c.isSingleConsecutiveDay()) {
                newHistory.consecutiveDayoffNums[nurse] += history.consecutiveDayoffNums[nurse];
            }
        }
    }

    return newHistory;
}



void NurseRostering::TabuSolver::Solution::AvailableNurses::setEnvironment( int w, SkillID s )
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

void NurseRostering::TabuSolver::Solution::AvailableNurses::setShift( ShiftID s )
{
    shift = s;
    minSkillNum = 0;
    validNurseNum_CurShift = validNurseNum_CurDay;
}

NurseRostering::NurseID NurseRostering::TabuSolver::Solution::AvailableNurses::getNurse()
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
            if (sln.getAssign().isWorking( nurse, weekday )) { // set the nurse invalid for current day
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
