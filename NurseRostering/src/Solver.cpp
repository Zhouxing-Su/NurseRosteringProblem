#include "Solver.h"


using namespace std;


const Timer::Duration NurseRostering::Solver::SAVE_SOLUTION_TIME = Timer::Duration( 500 );
const double NurseRostering::Solver::INIT_PERTURB_STRENGTH = 0.2;
const double NurseRostering::Solver::PERTURB_STRENGTH_DELTA = 0.01;
const double NurseRostering::Solver::MAX_PERTURB_STRENGTH = 0.6;



NurseRostering::Solver::Solver( const NurseRostering &input, Timer::TimePoint st )
    : problem( input ), startTime( st ), timer( problem.timeout, startTime ), sln( *this ), dayTabuTenureBase( 1 ), shiftTabuTenureBase( 1 )
{
}

NurseRostering::Solver::Solver( const NurseRostering &input, const Output &opt, Timer::TimePoint st )
    : problem( input ), startTime( st ), optima( opt ), timer( problem.timeout, startTime ), sln( *this ), dayTabuTenureBase( 1 ), shiftTabuTenureBase( 1 )
{
}

bool NurseRostering::Solver::check() const
{
    bool feasible = (checkFeasibility() == 0);
    bool objValMatch = (checkObjValue() == optima.getObjValue());

    if (!feasible) {
        errorLog( "infeasible optima solution." );
    }
    if (!objValMatch) {
        errorLog( "obj value does not match in optima solution." );
    }

    return (feasible && objValMatch);
}

NurseRostering::ObjValue NurseRostering::Solver::checkFeasibility( const AssignTable &assign ) const
{
    ObjValue objValue = 0;
    NurseNumsOnSingleAssign nurseNum( countNurseNums( assign ) );

    // check H1: Single assignment per day
    // always true

    // check H2: Under-staffing
    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        for (ShiftID shift = NurseRostering::Scenario::Shift::ID_BEGIN; shift < problem.scenario.shiftSize; ++shift) {
            for (SkillID skill = NurseRostering::Scenario::Skill::ID_BEGIN; skill < problem.scenario.skillSize; ++skill) {
                if (nurseNum[weekday][shift][skill] < problem.weekData.minNurseNums[weekday][shift][skill]) {
                    objValue += DefaultPenalty::UnderStaff_Repair *
                        (problem.weekData.minNurseNums[weekday][shift][skill] - nurseNum[weekday][shift][skill]);
                }
            }
        }
    }

    // check H3: Shift type successions
    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            objValue += DefaultPenalty::Succession_Repair *
                (!problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[assign[nurse][weekday].shift]);
        }
    }

    // check H4: Missing required skill
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            if (!problem.scenario.nurses[nurse].skills[assign[nurse][weekday].skill]) {
                return DefaultPenalty::FORBIDDEN_MOVE;
            }
        }
    }

    return objValue;
}

NurseRostering::ObjValue NurseRostering::Solver::checkFeasibility() const
{
    return checkFeasibility( optima.getAssignTable() );
}

NurseRostering::ObjValue NurseRostering::Solver::checkObjValue( const AssignTable &assign ) const
{
    ObjValue objValue = 0;
    NurseNumsOnSingleAssign nurseNums( countNurseNums( assign ) );

    // check S1: Insufficient staffing for optimal coverage (30)
    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        for (ShiftID shift = NurseRostering::Scenario::Shift::ID_BEGIN; shift < problem.scenario.shiftSize; ++shift) {
            for (SkillID skill = NurseRostering::Scenario::Skill::ID_BEGIN; skill < problem.scenario.skillSize; ++skill) {
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

        for (int weekday = Weekday::Tue; weekday <= Weekday::Sun; ++weekday) {
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
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
            objValue += DefaultPenalty::Preference *
                problem.weekData.shiftOffs[weekday][shift][nurse];
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
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        int min = problem.scenario.nurses[nurse].restMinShiftNum;
        int max = problem.scenario.nurses[nurse].restMaxShiftNum;
        int assignNum = 0;
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            assignNum += assign.isWorking( nurse, weekday );
        }
#ifdef INRC2_LOG
        if (problem.history.currentWeek == problem.scenario.totalWeekNum) {
#endif
            objValue += DefaultPenalty::TotalAssign * distanceToRange(
                assignNum * problem.history.restWeekCount, min, max ) / problem.history.restWeekCount;

#ifdef INRC2_AVERAGE_MAX_WORKING_WEEKEND
            int maxWeekend = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
            int historyWeekend = problem.history.totalWorkingWeekendNums[nurse] * problem.scenario.totalWeekNum;
            int exceedingWeekend = historyWeekend - (maxWeekend * problem.history.currentWeek) +
                ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * problem.scenario.totalWeekNum);
            if (exceedingWeekend > 0) {
                objValue += DefaultPenalty::TotalWorkingWeekend * exceedingWeekend / problem.scenario.totalWeekNum;
            }
#else
            int workingWeekendNum = (assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun ));
            objValue += DefaultPenalty::TotalWorkingWeekend * exceedCount(
                workingWeekendNum * problem.history.restWeekCount,
                problem.scenario.nurses[nurse].restMaxWorkingWeekendNum ) / problem.history.restWeekCount;
#endif
#ifdef INRC2_LOG
        }
#endif
    }

    return objValue;
}

NurseRostering::ObjValue NurseRostering::Solver::checkObjValue() const
{
    return checkObjValue( optima.getAssignTable() );
}

void NurseRostering::Solver::print() const
{
    cout << "optima.objVal: " << (optima.getObjValue() / DefaultPenalty::AMP) << endl;
}

void NurseRostering::Solver::initResultSheet( std::ofstream &csvFile )
{
    csvFile << "Time,ID,Instance,Algorithm,RandSeed,GenCount,IterCount,Duration,Feasible,Check-Obj,ObjValue,AccObjValue,Solution" << std::endl;
}

void NurseRostering::Solver::record( const std::string logFileName, const std::string &instanceName ) const
{
    // create the log file if it does not exist
    ofstream csvFile( logFileName, ios::app );
    csvFile.close();

    // wait if others are writing to log file
    logFileMutex.lock();

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
        << "TSR" << ","
        << problem.randSeed << ","
        << generationCount << ","
        << iterationCount << ","
        << chrono::duration_cast<std::chrono::milliseconds>(optima.getFindTime() - startTime).count() / 1000.0 << "s,"
        << checkFeasibility() << ","
        << (checkObjValue() - optima.getObjValue()) / static_cast<double>(DefaultPenalty::AMP) << ","
        << optima.getObjValue() / static_cast<double>(DefaultPenalty::AMP) << ","
        << (optima.getObjValue() + problem.history.accObjValue) / static_cast<double>(DefaultPenalty::AMP) << ",";

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            csvFile << optima.getAssign( nurse, weekday ).shift << ' '
                << optima.getAssign( nurse, weekday ).skill << ' ';
        }
    }

    csvFile << endl;
    csvFile.close();
    logFileMutex.unlock();
}

void NurseRostering::Solver::errorLog( const std::string &msg ) const
{
#ifdef INRC2_LOG
    cerr << getTime() << "," << runID << "," << msg << endl;
#endif
}


NurseRostering::NurseNumsOnSingleAssign NurseRostering::Solver::countNurseNums( const AssignTable &assign ) const
{
    NurseNumsOnSingleAssign nurseNums( Weekday::SIZE,
        vector< vector<int> >( problem.scenario.shiftSize, vector<int>( problem.scenario.skillSize, 0 ) ) );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            ++nurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];
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
    if (Assign::isWorking( shift )) {    // working day
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
    nurseNumOfSkill = vector<SkillID>( problem.scenario.skillSize, 0 );
    nurseWithSkill = vector< vector< vector<NurseID> > >( problem.scenario.skillSize );
    nursesHasSameSkill = vector< vector<bool> >( problem.scenario.nurseNum, vector<bool>( problem.scenario.nurseNum ) );

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        const vector<bool> &skills = problem.scenario.nurses[nurse].skills;
        unsigned skillNum = problem.scenario.nurses[nurse].skillNum;
        for (SkillID skill = NurseRostering::Scenario::Skill::ID_BEGIN; skill < problem.scenario.skillSize; ++skill) {
            if (skills[skill]) {
                ++nurseNumOfSkill[skill];
                if (skillNum > nurseWithSkill[skill].size()) {
                    nurseWithSkill[skill].resize( skillNum );
                }
                nurseWithSkill[skill][skillNum - 1].push_back( nurse );
            }
        }
        for (NurseID nurse2 = 0; nurse2 < problem.scenario.nurseNum; ++nurse2) {
            nursesHasSameSkill[nurse][nurse2] = problem.haveSameSkill( nurse, nurse2 );
        }
    }
}




void NurseRostering::Solver::init( const std::string &id )
{
    runID = id;
    iterationCount = 0;
    generationCount = 0;
    randGen.seed( problem.randSeed );

    setTabuTenure();
    setMaxNoImprove();

    discoverNurseSkillRelation();

    sln.genInitAssign();
    optima = sln;
}

void NurseRostering::Solver::solve()
{
    biasTabuSearch();
    //tabuSearch( MaxNoImproveForAllNeighborhood() );

#ifdef INRC2_LOG
    if (problem.history.currentWeek < problem.scenario.totalWeekNum) {
        sln.rebuild( optima );
        sln.evaluateObjValue( false );
        optima = Output( sln.getObjValue(), sln.getAssignTable(),
            optima.getSecondaryObjValue(), optima.getFindTime() );
    }
#endif
}

bool NurseRostering::Solver::updateOptima( const Output &localOptima )
{
    if (localOptima.getObjValue() < optima.getObjValue()) {
        optima = localOptima;
        return true;
    } else if (localOptima.getObjValue() == optima.getObjValue()) {
#ifdef INRC2_SECONDARY_OBJ_VALUE
        bool  isSelected = (localOptima.getSecondaryObjValue() < optima.getSecondaryObjValue());
#else
        bool isSelected = ((randGen() % 2) == 0);
#endif
        if (isSelected) {
            optima = localOptima;
            return true;
        }
    }

    return false;
}

NurseRostering::History NurseRostering::Solver::genHistory() const
{
    return Solution( *this, optima ).genHistory();
}

void NurseRostering::Solver::tabuSearch( IterCount maxNoImproveCount )
{
    double perturbStrength = INIT_PERTURB_STRENGTH;
    double perturbStrengthDelta = PERTURB_STRENGTH_DELTA;
    while (!timer.isTimeOut()) {
        sln.tabuSearch_Rand( timer, maxNoImproveCount );
        ++generationCount;

        if (updateOptima( sln.getOptima() )) {
#ifdef INRC2_INC_PERTURB_STRENGTH_DELTA
            perturbStrengthDelta = PERTURB_STRENGTH_DELTA;
#endif
            perturbStrength = INIT_PERTURB_STRENGTH;
        } else if (perturbStrength < MAX_PERTURB_STRENGTH) {
#ifdef INRC2_INC_PERTURB_STRENGTH_DELTA
            perturbStrengthDelta += PERTURB_STRENGTH_DELTA;
#endif
            perturbStrength += perturbStrengthDelta;
        }
        const Output &output( (randGen() % PERTURB_ORIGIN_SELECT)
            ? optima : sln.getOptima() );
        sln.rebuild( output, perturbStrength );
    }

    iterationCount = sln.getIterCount();
}

void NurseRostering::Solver::biasTabuSearch()
{
    while (!timer.isTimeOut()) {
        sln.tabuSearch_Rand( timer, MaxNoImproveForAllNeighborhood() );

        updateOptima( sln.getOptima() );
        const Output &output( (randGen() % BIAS_TS_ORIGIN_SELECT)
            ? optima : sln.getOptima() );
        sln.rebuild( output );

        sln.adjustWeightToBiasNurseWithGreaterPenalty( INVERSE_TOTAL_BIAS_RATIO, INVERSE_PENALTY_BIAS_RATIO );
        sln.tabuSearch_Rand( timer, MaxNoImproveForBiasTabuSearch() );
        ++generationCount;
        sln.rebuild( sln.getOptima() );
        sln.evaluateObjValue();
    }

    iterationCount = sln.getIterCount();
}

void NurseRostering::Solver::setTabuTenure()
{
    dayTabuTenureBase = 1 + Weekday::NUM;
    shiftTabuTenureBase = 1 + Weekday::NUM;

    dayTabuTenureAmp = 1 + dayTabuTenureBase / TABU_BASE_TO_AMP;
    shiftTabuTenureAmp = 1 + shiftTabuTenureBase / TABU_BASE_TO_AMP;
}
