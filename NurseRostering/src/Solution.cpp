#include "Solution.h"
#include "Solver.h"


using namespace std;


NurseRostering::Solution::Solution( const Solver &s ) : solver( s ) {}

NurseRostering::Solution::Solution( const Solver &s, const AssignTable &a )
    : solver( s )
{
    resetAssign();
    rebuildAssistData( a );
}

void NurseRostering::Solution::rebuildAssistData( const AssignTable &a )
{
    if (&a == &assign) {
        solver.errorLog( "self assignment is not allowed!" );
    }

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (AssignTable::isWorking( a[nurse][weekday].shift )) {
                addAssign( weekday, nurse, a[nurse][weekday] );
            }
        }
    }
    evaluateObjValue();
}

bool NurseRostering::Solution::genInitAssign_Greedy()
{
    resetAssign();

    AvailableNurses availableNurse( *this );
    const NurseNumOfSkill &nurseNumOfSkill( solver.getNurseNumOfSkill() );

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
            dailyRequire[skill] /= nurseNumOfSkill[skill];
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

    Timer timer( solver.problem.timeout, solver.startTime );
    bool feasible = fillAssign( timer, Weekday::Mon, 0, 0, 0, 0 );
    AssignTable a( assign );
    rebuildAssistData( a );
    return feasible;
}

bool NurseRostering::Solution::fillAssign( const Timer &timer, int weekday, ShiftID shift, SkillID skill, NurseID nurse, int nurseNum )
{
    if (nurse >= solver.problem.scenario.nurseNum) {
        if (nurseNum < solver.problem.weekData.minNurseNums[weekday][shift][skill]) {
            return false;
        } else {
            return fillAssign( timer, weekday, shift, skill + 1, 0, 0 );
        }
    } else if (skill >= solver.problem.scenario.skillTypeNum) {
        return fillAssign( timer, weekday, shift + 1, 0, 0, 0 );
    } else if (shift >= solver.problem.scenario.shiftTypeNum) {
        return fillAssign( timer, weekday + 1, 0, 0, 0, 0 );
    } else if (weekday > Weekday::Sun) {
        return true;
    }

    if (timer.isTimeOut()) {
        return false;
    }

    Assign firstAssign( shift, skill );
    Assign secondAssign;
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
            if (fillAssign( timer, weekday, shift, skill, nurse + 1, firstNurseNum )) {
                return true;
            }
        }

        assign[nurse][weekday] = secondAssign;
    }

    if (fillAssign( timer, weekday, shift, skill, nurse + 1, secondNurseNum )) {
        return true;
    } else if (isNotAssignedBefore) {
        assign[nurse][weekday] = Assign();
    }

    return false;
}


void NurseRostering::Solution::resetAssign()
{
    assign = AssignTable( solver.problem.scenario.nurseNum, Weekday::SIZE );
    missingNurseNums = solver.problem.weekData.optNurseNums;
    totalAssignNums = solver.problem.history.totalAssignNums;
    consecutives = vector<Consecutive>( solver.problem.scenario.nurseNum );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        consecutives[nurse] = Consecutive( solver.problem.history, nurse );
        assign[nurse][Weekday::HIS] = Assign( solver.problem.history.lastShifts[nurse] );
    }
}

void NurseRostering::Solution::evaluateObjValue()
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

bool NurseRostering::Solution::repair( const Timer &timer )
{
    // TODO
    bool feasible;
    do {
        feasible = genInitAssign_Greedy();
    } while (!feasible && !timer.isTimeOut());

    return (feasible || genInitAssign_BranchAndCut());
}

void NurseRostering::Solution::localSearch( const Timer &timer, Output &optima )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
#endif

    long long iterCount = 0;
    int failCount = 0;

    while (!timer.isTimeOut() && (failCount < 3)) {
        Move bestMove;
        while (!timer.isTimeOut() && findBestAddShift( bestMove )) {
            addAssign( bestMove );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestChangeShift( bestMove )) {
            changeAssign( bestMove );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestRemoveShift( bestMove )) {
            removeAssign( bestMove );
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
    cout << "iter: " << iterCount << ' '
        << "time: " << duration << ' '
        << "speed: " << iterCount * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::Solution::localSearchOnConsecutiveBorder( const Timer &timer, Output &optima )
{
#ifdef INRC2_PERFORMANCE_TEST
    clock_t startTime = clock();
#endif

    long long iterCount = 0;
    int failCount = 0;

    while (!timer.isTimeOut() && (failCount < 3)) {
        Move bestMove;
        while (!timer.isTimeOut() && findBestAddShiftOnConsecutiveBorder( bestMove )) {
            addAssign( bestMove );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestChangeShiftOnConsecutiveBorder( bestMove )) {
            changeAssign( bestMove );
            objValue += bestMove.delta;
            if (objValue < optima.objVal) {
                optima = genOutput();
            }
            ++iterCount;
            failCount = 0;
        }
        ++failCount;

        while (!timer.isTimeOut() && findBestRemoveShiftOnConsecutiveBorder( bestMove )) {
            removeAssign( bestMove );
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
    cout << "iter: " << iterCount << ' '
        << "time: " << duration << ' '
        << "speed: " << iterCount * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

void NurseRostering::Solution::randomWalk( const Timer &timer, Output &optima )
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
        Assign a( rand() % solver.problem.scenario.shiftTypeNum,
            rand() % solver.problem.scenario.skillTypeNum );
        switch (select) {
            case 0:
                delta = tryAddAssign( weekday, nurse, a );
                if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                    objValue += delta;
                    addAssign( weekday, nurse, a );
                }
                break;
            case 1:
                delta = tryChangeAssign( weekday, nurse, a );
                if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                    objValue += delta;
                    changeAssign( weekday, nurse, a );
                }
                break;
            case 2:
                delta = tryRemoveAssign( weekday, nurse );
                if (delta < DefaultPenalty::MAX_OBJ_VALUE) {
                    objValue += delta;
                    removeAssign( weekday, nurse );
                }
                break;
        }

        if (objValue < optima.objVal) {
            optima = genOutput();
        }
    }
#ifdef INRC2_PERFORMANCE_TEST
    clock_t duration = clock() - startTime;
    cout << "iter: " << iterCount << ' '
        << "time: " << duration << ' '
        << "speed: " << iterCount * static_cast<double>(CLOCKS_PER_SEC) / (duration + 1) << endl;
#endif
}

bool NurseRostering::Solution::findBestAddShift( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = DefaultPenalty::MAX_OBJ_VALUE;

    // search for best addShift
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (!assign.isWorking( nurse, weekday )) {
                Assign a;
                for (a.shift = 0; a.shift < solver.problem.scenario.shiftTypeNum; ++a.shift) {
                    for (a.skill = 0; a.skill < solver.problem.scenario.skillTypeNum; ++a.skill) {
                        ObjValue delta = tryAddAssign( weekday, nurse, a );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, a );
                        }
                    }
                }
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestChangeShift( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = DefaultPenalty::MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
                Assign a;
                for (a.shift = 0; a.shift < solver.problem.scenario.shiftTypeNum; ++a.shift) {
                    for (a.skill = 0; a.skill < solver.problem.scenario.skillTypeNum; ++a.skill) {
                        ObjValue delta = tryChangeAssign( weekday, nurse, a );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, a );
                        }
                    }
                }
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestRemoveShift( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = DefaultPenalty::MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
                ObjValue delta = tryRemoveAssign( weekday, nurse );
                if (rs.isMinimal( delta, bestMove.delta )) {
                    bestMove = Move( delta, nurse, weekday );
                }
            }
        }
    }

    return (bestMove.delta < 0);
}

bool NurseRostering::Solution::findBestAddShiftOnConsecutiveBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = DefaultPenalty::MAX_OBJ_VALUE;

    // search for best addShift
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE;) {
            if (!assign.isWorking( nurse, weekday )) {
                Assign a;
                for (a.shift = 0; a.shift < solver.problem.scenario.shiftTypeNum; ++a.shift) {
                    for (a.skill = 0; a.skill < solver.problem.scenario.skillTypeNum; ++a.skill) {
                        ObjValue delta = tryAddAssign( weekday, nurse, a );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, a );
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

bool NurseRostering::Solution::findBestChangeShiftOnConsecutiveBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = DefaultPenalty::MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE;) {
            if (assign.isWorking( nurse, weekday )) {
                Assign a;
                for (a.shift = 0; a.shift < solver.problem.scenario.shiftTypeNum; ++a.shift) {
                    for (a.skill = 0; a.skill < solver.problem.scenario.skillTypeNum; ++a.skill) {
                        ObjValue delta = tryChangeAssign( weekday, nurse, a );
                        if (rs.isMinimal( delta, bestMove.delta )) {
                            bestMove = Move( delta, nurse, weekday, a );
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

bool NurseRostering::Solution::findBestRemoveShiftOnConsecutiveBorder( Move &bestMove ) const
{
    RandSelect<ObjValue> rs;
    bestMove.delta = DefaultPenalty::MAX_OBJ_VALUE;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE;) {
            if (assign.isWorking( nurse, weekday )) {
                ObjValue delta = tryRemoveAssign( weekday, nurse );
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

bool NurseRostering::Solution::isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const
{
    return (!assign.isWorking( nurse, weekday - 1 )
        || solver.problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[shift]);
}

bool NurseRostering::Solution::isValidPrior( NurseID nurse, ShiftID shift, int weekday ) const
{
    return ((weekday >= Weekday::Sun) || (!assign.isWorking( nurse, weekday + 1 ))
        || solver.problem.scenario.shifts[shift].legalNextShifts[assign[nurse][weekday + 1].shift]);
}

NurseRostering::ObjValue NurseRostering::Solution::tryAddAssign( int weekday, NurseID nurse, const Assign &a ) const
{
    ObjValue delta = 0;

    ShiftID oldShiftID = assign[nurse][weekday].shift;
    // TODO : make sure they won't be the same and leave out this
    if (!AssignTable::isWorking( a.shift ) || (a.shift == oldShiftID)
        || AssignTable::isWorking( oldShiftID )) {
        return DefaultPenalty::MAX_OBJ_VALUE;
    }

    // hard constraint check
    if (!solver.problem.scenario.nurses[nurse].skills[a.skill]) {
        delta += penalty.MissSkill();
    }

    if (!(isValidSuccession( nurse, a.shift, weekday )
        && isValidPrior( nurse, a.shift, weekday ))) {
        delta += penalty.Succession();
    }

    if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
        return delta;
    }

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    ContractID contractID = solver.problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;
    int currentWeek = solver.problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta -= penalty.InsufficientStaff() *
        (missingNurseNums[weekday][a.shift][a.skill] > 0);

    // consecutive shift
    const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );
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
    const WeekData &weekData( solver.problem.weekData );
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
            const History &history( solver.problem.history );
            delta -= penalty.TotalWorkingWeekend() * exceedCount(
                history.totalWorkingWeekendNums[nurse] * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
            delta += penalty.TotalWorkingWeekend() * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
        }
    }

    // total assign
    delta -= penalty.TotalAssign() * distanceToRange(
        totalAssignNums[nurse] * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;
    delta += penalty.TotalAssign() * distanceToRange(
        (totalAssignNums[nurse] + 1) * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;

    return delta;
}

NurseRostering::ObjValue NurseRostering::Solution::tryChangeAssign( int weekday, NurseID nurse, const Assign &a ) const
{
    ObjValue delta = 0;

    ShiftID oldShiftID = assign[nurse][weekday].shift;
    SkillID oldSkillID = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if (!AssignTable::isWorking( a.shift ) || !AssignTable::isWorking( oldShiftID )
        || ((a.shift == oldShiftID) && (a.skill == oldSkillID))) {
        return DefaultPenalty::MAX_OBJ_VALUE;
    }

    if (!solver.problem.scenario.nurses[nurse].skills[a.skill]) {
        delta += penalty.MissSkill();
    }

    if (!(isValidSuccession( nurse, a.shift, weekday )
        && isValidPrior( nurse, a.shift, weekday ))) {
        delta += penalty.Succession();
    }

    const WeekData &weekData( solver.problem.weekData );
    int lack = weekData.minNurseNums[weekday][oldShiftID][oldSkillID] -
        (weekData.optNurseNums[weekday][oldShiftID][oldSkillID] -
        missingNurseNums[weekday][oldShiftID][oldSkillID]);
    if (lack >= 0) {
        delta += penalty.UnderStaff() * lack;
    }

    if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
        return delta;
    }

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
        const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );
        const Scenario::Shift &shift( shifts[a.shift] );
        const Scenario::Shift &oldShift( solver.problem.scenario.shifts[oldShiftID] );
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

NurseRostering::ObjValue NurseRostering::Solution::tryRemoveAssign( int weekday, NurseID nurse ) const
{
    ObjValue delta = 0;

    ShiftID oldShiftID = assign[nurse][weekday].shift;
    SkillID oldSkillID = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if (!AssignTable::isWorking( oldShiftID )) {
        return DefaultPenalty::MAX_OBJ_VALUE;
    }

    const WeekData &weekData( solver.problem.weekData );
    int lack = weekData.minNurseNums[weekday][oldShiftID][oldSkillID] -
        (weekData.optNurseNums[weekday][oldShiftID][oldSkillID] -
        missingNurseNums[weekday][oldShiftID][oldSkillID]);
    if (lack >= 0) {
        delta += penalty.UnderStaff() * lack;
    }

    if (delta >= DefaultPenalty::MAX_OBJ_VALUE) {
        return delta;
    }

    int prevDay = weekday - 1;
    int nextDay = weekday + 1;
    ContractID contractID = solver.problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;
    int currentWeek = solver.problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    // insufficient staff
    delta += penalty.InsufficientStaff() *
        (missingNurseNums[weekday][oldShiftID][oldSkillID] >= 0);

    // consecutive shift
    const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );
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
            const History &history( solver.problem.history );
            delta -= penalty.TotalWorkingWeekend() * exceedCount(
                (history.totalWorkingWeekendNums[nurse] + 1) * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
            delta += penalty.TotalWorkingWeekend() * exceedCount(
                history.totalWorkingWeekendNums[nurse] * totalWeekNum,
                contract.maxWorkingWeekendNum * currentWeek ) / totalWeekNum;
        }
    }

    // total assign
    delta -= penalty.TotalAssign() * distanceToRange(
        totalAssignNums[nurse] * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;
    delta += penalty.TotalAssign() * distanceToRange(
        (totalAssignNums[nurse] - 1) * totalWeekNum, contract.minShiftNum * currentWeek,
        contract.maxShiftNum * currentWeek ) / totalWeekNum;

    return delta;
}

NurseRostering::ObjValue NurseRostering::Solution::trySwapNurse( int weekday, NurseID nurse1, NurseID nurse2 ) const
{
    ObjValue delta = 0;
    penalty.setSwapMode();

    if (assign.isWorking( nurse1, weekday )) {
        if (assign.isWorking( nurse2, weekday )) {
            delta += tryChangeAssign( weekday, nurse1, assign[nurse2][weekday] );
            delta += tryChangeAssign( weekday, nurse2, assign[nurse1][weekday] );
        } else {
            delta += tryRemoveAssign( weekday, nurse1 );
            delta += tryAddAssign( weekday, nurse2, assign[nurse1][weekday] );
        }
    } else {
        if (assign.isWorking( nurse2, weekday )) {
            delta += tryAddAssign( weekday, nurse1, assign[nurse2][weekday] );
            delta += tryRemoveAssign( weekday, nurse2 );
        } else {    // no change
            delta += DefaultPenalty::MAX_OBJ_VALUE;
        }
    }

    penalty.setNormalMode();
    return delta;
}

void NurseRostering::Solution::addAssign( int weekday, NurseID nurse, const Assign &a )
{
    updateConsecutive( weekday, nurse, a.shift );

    --missingNurseNums[weekday][a.shift][a.skill];

    ++totalAssignNums[nurse];

    assign[nurse][weekday] = Assign( a );
}

void NurseRostering::Solution::addAssign( const Move &move )
{
    addAssign( move.weekday, move.nurse, move.assign );
}

void NurseRostering::Solution::changeAssign( int weekday, NurseID nurse, const Assign &a )
{
    if (a.shift != assign[nurse][weekday].shift) {
        updateConsecutive( weekday, nurse, a.shift );
    }

    --missingNurseNums[weekday][a.shift][a.skill];
    ++missingNurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];

    assign[nurse][weekday] = Assign( a );
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

void NurseRostering::Solution::swapNurse( int weekday, NurseID nurse1, NurseID nurse2 )
{
    if (assign.isWorking( nurse1, weekday )) {
        if (assign.isWorking( nurse2, weekday )) {
            changeAssign( weekday, nurse1, assign[nurse2][weekday] );
            changeAssign( weekday, nurse2, assign[nurse1][weekday] );
        } else {
            removeAssign( weekday, nurse1 );
            addAssign( weekday, nurse2, assign[nurse1][weekday] );
        }
    } else {
        if (assign.isWorking( nurse2, weekday )) {
            addAssign( weekday, nurse1, assign[nurse2][weekday] );
            removeAssign( weekday, nurse2 );
        }
    }
}

void NurseRostering::Solution::updateConsecutive( int weekday, NurseID nurse, ShiftID shift )
{
    Consecutive &c( consecutives[nurse] );
    int nextDay = weekday + 1;
    int prevDay = weekday - 1;

    // consider day
    bool isDayHigh = (weekday == c.dayHigh[weekday]);
    bool isDayLow = (weekday == c.dayLow[weekday]);
    if (assign.isWorking( nurse, weekday ) != AssignTable::isWorking( shift )) {
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

bool NurseRostering::Solution::checkIncrementalUpdate()
{
    ObjValue incrementalVal = objValue;
    evaluateObjValue();
    if (!solver.checkFeasibility( assign )) {
        solver.errorLog( "infeasible solution." );
        return false;
    }
    ObjValue checkResult = solver.checkObjValue( assign );
    if (checkResult != objValue) {
        solver.errorLog( "check conflict with evaluate." );
        return false;
    }
    if (objValue != incrementalVal) {
        solver.errorLog( "evaluate conflict with incremental update." );
        return false;
    }

    return true;
}

void NurseRostering::Solution::evaluateInsufficientStaff()
{
    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
        for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
                if (missingNurseNums[weekday][shift][skill] > 0) {
                    objInsufficientStaff += penalty.InsufficientStaff() * missingNurseNums[weekday][shift][skill];
                }
            }
        }
    }
}

void NurseRostering::Solution::evaluateConsecutiveShift()
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
                        objConsecutiveShift += penalty.ConsecutiveShift() * (c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1);
                    } else {
                        objConsecutiveShift += penalty.ConsecutiveShift() * distanceToRange(
                            c.shiftHigh[Weekday::Mon] - c.shiftLow[Weekday::Mon] + 1,
                            shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    }
                } else {
                    objConsecutiveShift += penalty.ConsecutiveShift() * distanceToRange(
                        c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1, shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    if (AssignTable::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += penalty.ConsecutiveShift() * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else if (AssignTable::isWorking( history.lastShifts[nurse] )) {
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
            const Scenario::Shift &shift( solver.problem.scenario.shifts[shiftID] );
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
                    if (AssignTable::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += penalty.ConsecutiveShift() * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else {
                objConsecutiveShift += penalty.ConsecutiveShift() * exceedCount(
                    consecutiveShift, shift.maxConsecutiveShiftNum );
            }
        } else if (c.isSingleConsecutiveShift() // the entire week is one block
            && AssignTable::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveShift += penalty.ConsecutiveShift() * absentCount(
                history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
        }
    }
}

void NurseRostering::Solution::evaluateConsecutiveDay()
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
                    objConsecutiveDay += penalty.ConsecutiveDay() * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDay += penalty.ConsecutiveDay() * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
            } else if (AssignTable::isWorking( history.lastShifts[nurse] )) {
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
            && AssignTable::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveDay += penalty.ConsecutiveDay() * absentCount(
                history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
        }
    }
}

void NurseRostering::Solution::evaluateConsecutiveDayOff()
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
                    objConsecutiveDayOff += penalty.ConsecutiveDayOff() * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDayOff += penalty.ConsecutiveDayOff() * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
            } else if (!AssignTable::isWorking( history.lastShifts[nurse] )) {
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
            && (!AssignTable::isWorking( history.lastShifts[nurse] ))) {
            objConsecutiveDayOff += penalty.ConsecutiveDayOff() * absentCount(
                history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
        }
    }
}

void NurseRostering::Solution::evaluatePreference()
{
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
            if (AssignTable::isWorking( shift )) {
                objPreference += penalty.Preference() *
                    solver.problem.weekData.shiftOffs[weekday][shift][nurse];
            }
        }
    }
}

void NurseRostering::Solution::evaluateCompleteWeekend()
{
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        objCompleteWeekend += penalty.CompleteWeekend() *
            (solver.problem.scenario.contracts[solver.problem.scenario.nurses[nurse].contract].completeWeekend
            && (assign.isWorking( nurse, Weekday::Sat )
            != assign.isWorking( nurse, Weekday::Sun )));
    }
}

void NurseRostering::Solution::evaluateTotalAssign()
{
    const Scenario &scenario( solver.problem.scenario );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;

    for (NurseID nurse = 0; nurse < scenario.nurseNum; ++nurse) {
        int min = scenario.contracts[scenario.nurses[nurse].contract].minShiftNum;
        int max = scenario.contracts[scenario.nurses[nurse].contract].maxShiftNum;
        objTotalAssign += penalty.TotalAssign() * distanceToRange(
            totalAssignNums[nurse] * totalWeekNum,
            min * solver.problem.history.currentWeek,
            max * solver.problem.history.currentWeek ) / totalWeekNum;
        // remove penalty in the history except the first week
        if (solver.problem.history.pastWeekCount > 0) {
            objTotalAssign -= penalty.TotalAssign() * distanceToRange(
                solver.problem.history.totalAssignNums[nurse] * totalWeekNum,
                min * solver.problem.history.pastWeekCount,
                max * solver.problem.history.pastWeekCount ) / totalWeekNum;
        }
    }
}

void NurseRostering::Solution::evaluateTotalWorkingWeekend()
{
    int totalWeekNum = solver.problem.scenario.totalWeekNum;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        int maxWeekend = solver.problem.scenario.contracts[solver.problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
        int historyWeekend = solver.problem.history.totalWorkingWeekendNums[nurse] * totalWeekNum;
        int exceedingWeekend = historyWeekend - (maxWeekend * solver.problem.history.currentWeek) +
            ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * totalWeekNum);
        if (exceedingWeekend > 0) {
            objTotalWorkingWeekend += penalty.TotalWorkingWeekend() *
                exceedingWeekend / totalWeekNum;
        }
        // remove penalty in the history except the first week
        if (solver.problem.history.pastWeekCount > 0) {
            historyWeekend -= maxWeekend * solver.problem.history.pastWeekCount;
            if (historyWeekend > 0) {
                objTotalWorkingWeekend -= penalty.TotalWorkingWeekend() *
                    historyWeekend / totalWeekNum;
            }
        }
    }
}

NurseRostering::History NurseRostering::Solution::genHistory() const
{
    const History &history( solver.problem.history );
    History newHistory;
    newHistory.lastShifts.resize( solver.problem.scenario.nurseNum );
    newHistory.consecutiveShiftNums.resize( solver.problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayNums.resize( solver.problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayoffNums.resize( solver.problem.scenario.nurseNum, 0 );

    newHistory.accObjValue = history.accObjValue + objValue;
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
