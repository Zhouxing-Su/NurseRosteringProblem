#include "Solver.h"


using namespace std;


const clock_t NurseRostering::Solver::SAVE_SOLUTION_TIME = CLOCKS_PER_SEC / 2;
const clock_t NurseRostering::Solver::REPAIR_TIMEOUT_IN_INIT = CLOCKS_PER_SEC * 2;



NurseRostering::Solver::Solver( const NurseRostering &input, clock_t st )
    : problem( input ), startTime( st )
{
}

NurseRostering::Solver::Solver( const NurseRostering &input, const Output &opt, clock_t st )
    : problem( input ), startTime( st ), optima( opt )
{
}

NurseRostering::History NurseRostering::Solver::genHistory() const
{
    return Solution( *this, optima.assign ).genHistory();
}

bool NurseRostering::Solver::check() const
{
    bool feasible = checkFeasibility();
    bool objValMatch = (checkObjValue() == optima.objVal);

    if (!feasible) {
        errorLog( "infeasible optima solution." );
    }
    if (!objValMatch) {
        errorLog( "obj value does not match in optima solution." );
    }

    return (feasible && objValMatch);
}

bool NurseRostering::Solver::checkFeasibility( const AssignTable &assign ) const
{
    NurseNumsOnSingleAssign nurseNum( countNurseNums( assign ) );

    // check H1: Single assignment per day
    // always true

    // check H2: Under-staffing
    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
        for (ShiftID shift = 0; shift < problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < problem.scenario.skillTypeNum; ++skill) {
                if (nurseNum[weekday][shift][skill] < problem.weekData.minNurseNums[weekday][shift][skill]) {
                    return false;
                }
            }
        }
    }

    // check H3: Shift type successions
    // first day check the history
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        if (assign.isWorking( nurse, Weekday::Mon )
            && (problem.history.lastShifts[nurse] != NurseRostering::Scenario::Shift::ID_NONE)) {
            if (!problem.scenario.shifts[problem.history.lastShifts[nurse]].legalNextShifts[assign[nurse][Weekday::Mon].shift]) {
                return false;
            }
        }
    }
    for (int weekday = Weekday::Tue; weekday < Weekday::SIZE; ++weekday) {
        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            if (assign.isWorking( nurse, weekday ) && assign.isWorking( nurse, weekday - 1 )) {
                if (!problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[assign[nurse][weekday].shift]) {
                    return false;
                }
            }
        }
    }

    // check H4: Missing required skill
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
                if (!problem.scenario.nurses[nurse].skills[assign[nurse][weekday].skill]) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool NurseRostering::Solver::checkFeasibility() const
{
    return checkFeasibility( optima.assign );
}

NurseRostering::ObjValue NurseRostering::Solver::checkObjValue( const AssignTable &assign ) const
{
    ObjValue objValue = 0;
    NurseNumsOnSingleAssign nurseNums( countNurseNums( assign ) );

    // check S1: Insufficient staffing for optimal coverage (30)
    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
        for (ShiftID shift = 0; shift < problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < problem.scenario.skillTypeNum; ++skill) {
                int missingNurse = (problem.weekData.optNurseNums[weekday][shift][skill]
                    - nurseNums[weekday][shift][skill]);
                if (missingNurse > 0) {
                    objValue += DefaultPenalty::InsufficientStaff * missingNurse;
                }
            }
        }
    }

    // check S2: Consecutive assignments (15/30)
    // check S3: Consecutive days off (30)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        int consecutiveShift = problem.history.consecutiveShiftNums[nurse];
        int consecutiveDay = problem.history.consecutiveDayNums[nurse];
        int consecutiveDayOff = problem.history.consecutiveDayoffNums[nurse];
        bool shiftBegin = (consecutiveShift != 0);
        bool dayBegin = (consecutiveDay != 0);
        bool dayoffBegin = (consecutiveDayOff != 0);

        checkConsecutiveViolation( objValue, assign, nurse, Weekday::Mon, problem.history.lastShifts[nurse],
            consecutiveShift, consecutiveDay, consecutiveDayOff,
            shiftBegin, dayBegin, dayoffBegin );

        for (int weekday = Weekday::Tue; weekday < Weekday::SIZE; ++weekday) {
            checkConsecutiveViolation( objValue, assign, nurse, weekday, assign[nurse][weekday - 1].shift,
                consecutiveShift, consecutiveDay, consecutiveDayOff,
                shiftBegin, dayBegin, dayoffBegin );
        }
        // since penalty was calculated when switching assign, the penalty of last 
        // consecutive assignments are not considered. so finish it here.
        const ContractID &contractID( problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
        if (dayoffBegin && problem.history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
            objValue += DefaultPenalty::ConsecutiveDayOff * Weekday::NUM;
        } else if (consecutiveDayOff > contract.maxConsecutiveDayoffNum) {
            objValue += DefaultPenalty::ConsecutiveDayOff *
                (consecutiveDayOff - contract.maxConsecutiveDayoffNum);
        } else if (consecutiveDayOff == 0) {    // working day
            if (shiftBegin && problem.history.consecutiveShiftNums[nurse] > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += DefaultPenalty::ConsecutiveShift * Weekday::NUM;
            } else if (consecutiveShift > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += DefaultPenalty::ConsecutiveShift *
                    (consecutiveShift - problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum);
            }
            if (dayBegin && problem.history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                objValue += DefaultPenalty::ConsecutiveDay * Weekday::NUM;
            } else if (consecutiveDay > contract.maxConsecutiveDayNum) {
                objValue += DefaultPenalty::ConsecutiveDay *
                    (consecutiveDay - contract.maxConsecutiveDayNum);
            }
        }
    }

    // check S4: Preferences (10)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
            if (AssignTable::isWorking( shift )) {
                objValue += DefaultPenalty::Preference *
                    problem.weekData.shiftOffs[weekday][shift][nurse];
            }
        }
    }

    // check S5: Complete weekend (30)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        objValue += DefaultPenalty::CompleteWeekend *
            (problem.scenario.contracts[problem.scenario.nurses[nurse].contract].completeWeekend
            && (assign.isWorking( nurse, Weekday::Sat )
            != assign.isWorking( nurse, Weekday::Sun )));
    }

    // check S6: Total assignments (20)
    // check S7: Total working weekends (30)
    int totalWeekNum = problem.scenario.totalWeekNum;
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        int min = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minShiftNum;
        int max = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxShiftNum;
        int assignNum = problem.history.totalAssignNums[nurse];
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            assignNum += assign.isWorking( nurse, weekday );
        }
        objValue += DefaultPenalty::TotalAssign * distanceToRange( assignNum * totalWeekNum,
            min * problem.history.currentWeek, max * problem.history.currentWeek ) / totalWeekNum;

        int maxWeekend = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
        int historyWeekend = problem.history.totalWorkingWeekendNums[nurse] * totalWeekNum;
        int exceedingWeekend = historyWeekend - (maxWeekend * problem.history.currentWeek) +
            ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * totalWeekNum);
        if (exceedingWeekend > 0) {
            objValue += DefaultPenalty::TotalWorkingWeekend * exceedingWeekend / totalWeekNum;
        }

        // remove penalty in the history except the first week
        if (problem.history.pastWeekCount > 0) {
            objValue -= DefaultPenalty::TotalAssign * distanceToRange(
                problem.history.totalAssignNums[nurse] * totalWeekNum,
                min * problem.history.pastWeekCount, max * problem.history.pastWeekCount ) / totalWeekNum;

            historyWeekend -= maxWeekend * problem.history.pastWeekCount;
            if (historyWeekend > 0) {
                objValue -= DefaultPenalty::TotalWorkingWeekend * historyWeekend / totalWeekNum;
            }
        }
    }

    return objValue;
}

NurseRostering::ObjValue NurseRostering::Solver::checkObjValue() const
{
    return checkObjValue( optima.assign );
}

void NurseRostering::Solver::print() const
{
    cout << "optima.objVal: " << (optima.objVal / DefaultPenalty::AMP) << endl;
}

void NurseRostering::Solver::initResultSheet( std::ofstream &csvFile )
{
    csvFile << "Time,ID,Instance,Algorithm,RandSeed,Duration,Feasible,Check-Obj,ObjValue,AccObjValue,Solution" << std::endl;
}

void NurseRostering::Solver::record( const std::string logFileName, const std::string &instanceName ) const
{
    // create the log file if it does not exist
    ofstream csvFile( logFileName, ios::app );
    csvFile.close();

    // wait if others are writing to log file
    FileLock fl( logFileName );
    fl.lock();

    csvFile.open( logFileName, ios::app );
    csvFile.seekp( 0, ios::beg );
    ios::pos_type begin = csvFile.tellp();
    csvFile.seekp( 0, ios::end );
    if (csvFile.tellp() == begin) {
        initResultSheet( csvFile );
    }

    csvFile << getTime() << ","
        << runID << ","
        << instanceName << ","
        << algorithmName << ","
        << problem.randSeed << ","
        << (optima.findTime - startTime) / static_cast<double>(CLOCKS_PER_SEC) << "s,"
        << checkFeasibility() << ","
        << (checkObjValue() - optima.objVal) / static_cast<double>(DefaultPenalty::AMP) << ","
        << optima.objVal / static_cast<double>(DefaultPenalty::AMP) << ","
        << (optima.objVal + problem.history.accObjValue) / static_cast<double>(DefaultPenalty::AMP) << ",";

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            csvFile << optima.assign[nurse][weekday].shift << ' '
                << optima.assign[nurse][weekday].skill << ' ';
        }
    }

    csvFile << endl;
    csvFile.close();
    fl.unlock();
}

void NurseRostering::Solver::errorLog( const std::string &msg ) const
{
#ifdef INRC2_DEBUG
    cerr << getTime() << "," << runID << "," << msg << endl;
#endif
}


NurseRostering::NurseNumsOnSingleAssign NurseRostering::Solver::countNurseNums( const AssignTable &assign ) const
{
    NurseNumsOnSingleAssign nurseNums( Weekday::SIZE,
        vector< vector<int> >( problem.scenario.shiftTypeNum, vector<int>( problem.scenario.skillTypeNum, 0 ) ) );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
                ++nurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];
            }
        }
    }

    return nurseNums;
}

void NurseRostering::Solver::checkConsecutiveViolation( int &objValue,
    const AssignTable &assign, NurseID nurse, int weekday, ShiftID lastShiftID,
    int &consecutiveShift, int &consecutiveDay, int &consecutiveDayOff,
    bool &shiftBegin, bool &dayBegin, bool &dayoffBegin ) const
{
    const ContractID &contractID = problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
    const ShiftID &shift = assign[nurse][weekday].shift;
    if (AssignTable::isWorking( shift )) {    // working day
        if (consecutiveDay == 0) {  // switch from consecutive day off to working
            if (dayoffBegin) {
                if (problem.history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                    objValue += DefaultPenalty::ConsecutiveDayOff * (weekday - Weekday::Mon);
                } else {
                    objValue += DefaultPenalty::ConsecutiveDayOff * distanceToRange( consecutiveDayOff,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
                dayoffBegin = false;
            } else {
                objValue += DefaultPenalty::ConsecutiveDayOff * distanceToRange( consecutiveDayOff,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            }
            consecutiveDayOff = 0;
            consecutiveShift = 1;
        } else {    // keep working
            if (shift == lastShiftID) {
                ++consecutiveShift;
            } else { // another shift
                const Scenario::Shift &lastShift( problem.scenario.shifts[lastShiftID] );
                if (shiftBegin) {
                    if (problem.history.consecutiveShiftNums[nurse] > lastShift.maxConsecutiveShiftNum) {
                        objValue += DefaultPenalty::ConsecutiveShift * (weekday - Weekday::Mon);
                    } else {
                        objValue += DefaultPenalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
                            lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                    }
                    shiftBegin = false;
                } else {
                    objValue += DefaultPenalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
                        lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                }
                consecutiveShift = 1;
            }
        }
        ++consecutiveDay;
    } else {    // day off
        if (consecutiveDayOff == 0) {   // switch from consecutive working to day off
            const Scenario::Shift &lastShift( problem.scenario.shifts[lastShiftID] );
            if (shiftBegin) {
                if (problem.history.consecutiveShiftNums[nurse] > lastShift.maxConsecutiveShiftNum) {
                    objValue += DefaultPenalty::ConsecutiveShift * (weekday - Weekday::Mon);
                } else {
                    objValue += DefaultPenalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                        lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                }
                shiftBegin = false;
            } else {
                objValue += DefaultPenalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                    lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
            }
            if (dayBegin) {
                if (problem.history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                    objValue += DefaultPenalty::ConsecutiveDay * (weekday - Weekday::Mon);
                } else {
                    objValue += DefaultPenalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
                dayBegin = false;
            } else {
                objValue += DefaultPenalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            }
            consecutiveShift = 0;
            consecutiveDay = 0;
        }
        ++consecutiveDayOff;
    }
}

void NurseRostering::Solver::discoverNurseSkillRelation()
{
    nurseNumOfSkill = vector<SkillID>( problem.scenario.skillTypeNum, 0 );
    nurseWithSkill = vector< vector< vector<NurseID> > >( problem.scenario.skillTypeNum );

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



NurseRostering::TabuSolver::TabuSolver( const NurseRostering &input, clock_t st )
    :Solver( input, st ), sln( *this )
{
}

NurseRostering::TabuSolver::TabuSolver( const NurseRostering &input, const Output &opt, clock_t st )
    : Solver( input, opt, st ), sln( *this )
{
}

void NurseRostering::TabuSolver::init( const string &id )
{
    runID = id;
    algorithmName = "Tabu";
    srand( problem.randSeed );

    discoverNurseSkillRelation();

    //exactInit();
    greedyInit();

    optima = sln.genOutput();
}

void NurseRostering::TabuSolver::solve()
{
    //randomWalk();
    iterativeLocalSearch();
}

void NurseRostering::TabuSolver::greedyInit()
{
    algorithmName += "[GreedyInit]";

    if (sln.genInitAssign_Greedy() == false) {
        Timer timer( REPAIR_TIMEOUT_IN_INIT, startTime );
        if (sln.repair( timer ) == false) {
            errorLog( "fail to generate feasible init solution." );
        }
    }
}

void NurseRostering::TabuSolver::exactInit()
{
    algorithmName += "[ExactInit]";

    if (sln.genInitAssign_BranchAndCut() == false) {
        errorLog( "no feasible solution!" );
    }
}

void NurseRostering::TabuSolver::randomWalk()
{
    algorithmName += "[RandomWalk]";

    Timer timer( problem.timeout, startTime );

    sln.randomWalk( timer, optima );
}

void NurseRostering::TabuSolver::iterativeLocalSearch()
{
    algorithmName += "[IterativeLocalSearch]";

    Timer timer( problem.timeout, startTime );

    const int timeForEachLoop = problem.timeout / PERTURB_COUNT_IN_ILS;
    int loopCount = timer.restTime() / timeForEachLoop;
    for (; !timer.isTimeOut() && loopCount > 0; --loopCount) {
        Timer t( timer.restTime() / loopCount, clock() );
        sln.randomWalk( t, optima );
        Solution::FindBestMoveTable &findBestMoveTable =
            (rand() % 2) ? Solution::findBestMove
            : Solution::findBestMoveOnConsecutiveBorder;
        sln.localSearch( timer, optima, findBestMoveTable );
    }
}
