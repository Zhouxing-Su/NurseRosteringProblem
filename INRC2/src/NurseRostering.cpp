#include "NurseRostering.h"


using namespace std;


const NurseRostering::ObjValue NurseRostering::MAX_OBJ_VALUE = 1000000000;

const NurseRostering::NurseID NurseRostering::Scenario::Nurse::ID_NONE = -1;

const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_ANY = -2;
const std::string NurseRostering::Scenario::Shift::NAME_ANY( "Any" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_NONE = -1;
const std::string NurseRostering::Scenario::Shift::NAME_NONE( "None" );


const clock_t NurseRostering::Solver::SAVE_SOLUTION_TIME = CLOCKS_PER_SEC / 2;
const clock_t NurseRostering::Solver::REPAIR_TIMEOUT_IN_INIT = CLOCKS_PER_SEC * 2;




NurseRostering::NurseRostering()
{
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_ANY] = NurseRostering::Scenario::Shift::ID_ANY;
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_NONE] = NurseRostering::Scenario::Shift::ID_NONE;
}



NurseRostering::Solver::Solver( const NurseRostering &input, const std::string &name, clock_t st )
    : problem( input ), algorithmName( name ), startTime( st )
{
}

bool NurseRostering::Solver::check() const
{
    return (checkFeasibility() && (checkObjValue() == optima.objVal));
}

bool NurseRostering::Solver::checkFeasibility( const Assign &assign ) const
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
                const vector<SkillID> &skills( problem.scenario.nurses[nurse].skills );
                if (find( skills.begin(), skills.end(),
                    assign[nurse][weekday].skill ) == skills.end()) {
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

NurseRostering::ObjValue NurseRostering::Solver::checkObjValue( const Assign &assign ) const
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
                    objValue += Penalty::InsufficientStaff * missingNurse;
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
            objValue += Penalty::ConsecutiveDayOff * Weekday::NUM;
        } else if (consecutiveDayOff > contract.maxConsecutiveDayoffNum) {
            objValue += Penalty::ConsecutiveDayOff *
                (consecutiveDayOff - contract.maxConsecutiveDayoffNum);
        } else if (consecutiveDayOff == 0) {    // working day
            if (shiftBegin && problem.history.consecutiveShiftNums[nurse] > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += Penalty::ConsecutiveShift * Weekday::NUM;
            } else if (consecutiveShift > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += Penalty::ConsecutiveShift *
                    (consecutiveShift - problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum);
            }
            if (dayBegin && problem.history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                objValue += Penalty::ConsecutiveDay * Weekday::NUM;
            } else if (consecutiveDay > contract.maxConsecutiveDayNum) {
                objValue += Penalty::ConsecutiveDay *
                    (consecutiveDay - contract.maxConsecutiveDayNum);
            }
        }
    }

    // check S4: Preferences (10)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
            if (Assign::isWorking( shift )) {
                objValue += Penalty::Preference *
                    problem.weekData.shiftOffs[weekday][shift][nurse];
            }
        }
    }

    // check S5: Complete weekend (30)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        objValue += Penalty::CompleteWeekend *
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
        objValue += Penalty::TotalAssign * distanceToRange( assignNum * totalWeekNum,
            min * problem.history.currentWeek, max * problem.history.currentWeek ) / totalWeekNum;

        int maxWeekend = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
        int historyWeekend = problem.history.totalWorkingWeekendNums[nurse] * totalWeekNum;
        int exceedingWeekend = historyWeekend - (maxWeekend * problem.history.currentWeek) +
            ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * totalWeekNum);
        if (exceedingWeekend > 0) {
            objValue += Penalty::TotalWorkingWeekend * exceedingWeekend / totalWeekNum;
        }

        // remove penalty in the history except the first week
        if (problem.history.pastWeekCount > 0) {
            objValue -= Penalty::TotalAssign * distanceToRange(
                problem.history.totalAssignNums[nurse] * totalWeekNum,
                min * problem.history.pastWeekCount, max * problem.history.pastWeekCount ) / totalWeekNum;

            historyWeekend -= maxWeekend * problem.history.pastWeekCount;
            if (historyWeekend > 0) {
                objValue -= Penalty::TotalWorkingWeekend * historyWeekend / totalWeekNum;
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
    cout << "optima.objVal: " << (optima.objVal / Penalty::AMP) << endl;
}

void NurseRostering::Solver::initResultSheet( std::ofstream &csvFile )
{
    csvFile << "Time, Instance, Algorithm, RandSeed, Duration, ObjValue, Solution" << std::endl;
}

void NurseRostering::Solver::record( const std::string logFileName, const std::string &instanceName ) const
{
    ofstream csvFile( logFileName, ios::app );
    csvFile.seekp( 0, ios::beg );
    ios::pos_type begin = csvFile.tellp();
    csvFile.seekp( 0, ios::end );
    if (csvFile.tellp() == begin) {
        initResultSheet( csvFile );
    }

    if (!check()) {
        csvFile << "[LogicError] ";
    }

    char timeBuf[64];
    time_t t = time( NULL );
    tm *date = localtime( &t );
    strftime( timeBuf, 64, "%Y-%m-%d %a %H:%M:%S", date );

    csvFile << timeBuf << ","
        << instanceName << ","
        << algorithmName << ","
        << problem.randSeed << ","
        << (optima.findTime - startTime) / static_cast<double>(CLOCKS_PER_SEC) << "s,"
        << (optima.objVal / static_cast<double>(Penalty::AMP)) << ",";

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (optima.assign.isWorking( nurse, weekday )) {
                csvFile << optima.assign[nurse][weekday].shift << ' '
                    << optima.assign[nurse][weekday].skill << ' ';
            }
        }
    }


    csvFile << endl;
    csvFile.close();
}

NurseRostering::NurseNumsOnSingleAssign NurseRostering::Solver::countNurseNums( const Assign &assign ) const
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
    const Assign &assign, NurseID nurse, int weekday, ShiftID lastShiftID,
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
                    objValue += Penalty::ConsecutiveDayOff * (weekday - Weekday::Mon);
                } else {
                    objValue += Penalty::ConsecutiveDayOff * distanceToRange( consecutiveDayOff,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
                dayoffBegin = false;
            } else {
                objValue += Penalty::ConsecutiveDayOff * distanceToRange( consecutiveDayOff,
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
                        objValue += Penalty::ConsecutiveShift * (weekday - Weekday::Mon);
                    } else {
                        objValue += Penalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
                            lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                    }
                    shiftBegin = false;
                } else {
                    objValue += Penalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
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
                    objValue += Penalty::ConsecutiveShift * (weekday - Weekday::Mon);
                } else {
                    objValue += Penalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                        lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                }
                shiftBegin = false;
            } else {
                objValue += Penalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                    lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
            }
            if (dayBegin) {
                if (problem.history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                    objValue += Penalty::ConsecutiveDay * (weekday - Weekday::Mon);
                } else {
                    objValue += Penalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
                dayBegin = false;
            } else {
                objValue += Penalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            }
            consecutiveShift = 0;
            consecutiveDay = 0;
        }
        ++consecutiveDayOff;
    }
}
