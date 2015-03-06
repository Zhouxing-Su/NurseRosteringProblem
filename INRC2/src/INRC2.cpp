#include "INRC2.h"


using namespace std;


void run( int argc, char *argv[], const std::string &instance )
{
    // handle command line arguments
    map<string, string> argvMap;
    for (int i = 1; i < argc; i += 2) {
        argvMap[argv[i] + 2] = argv[i + 1]; // (argv[i] + 2) to skip "--" before argument
    }

    NurseRostering input;
    NurseRostering::Solver *psolver = NULL;
    // recover scenario first, for week data may use information in it
    // if there is custom input, the context can be recovered more efficiently
    if (argvMap.find( ARGV_CUSTOM_INPUT ) != argvMap.end()) {
        psolver = readCustomInput( argvMap[ARGV_CUSTOM_INPUT] );
    } else {
        if (argvMap.find( ARGV_SCENARIO ) != argvMap.end()) {
            readScenario( argvMap[ARGV_SCENARIO], input );
        } else {
            cerr << "missing obligate argument(scenario)" << endl;
        }
        if (argvMap.find( ARGV_HISTORY ) != argvMap.end()) {
            readHistory( argvMap[ARGV_HISTORY], input );
        } else {
            cerr << "missing obligate argument(history)" << endl;
        }
    }
    if (argvMap.find( ARGV_WEEKDATA ) != argvMap.end()) {
        readWeekData( argvMap[ARGV_WEEKDATA], input );
    } else {
        cerr << "missing obligate argument(week data)" << endl;
    }
    if (argvMap.find( ARGV_SOLUTION ) != argvMap.end()) {
        input.solutionFileName = argvMap[ARGV_SOLUTION];
    } else {
        cerr << "missing obligate argument(solution file name)" << endl;
    }
    if (argvMap.find( ARGV_RANDOM_SEED ) != argvMap.end()) {
        istringstream iss( argvMap[ARGV_RANDOM_SEED] );
        iss >> input.randSeed;
    } else {
        input.randSeed = static_cast<int>(time( NULL ) + clock());
    }

    if (psolver == NULL) {

        // TODO
        psolver = new NurseRostering::TabuSolver( input );


    } else {    // update context

        // TODO



    }

    // start computation
    psolver->init();
    psolver->solve();
    psolver->check();
    psolver->record();
    if (!instance.empty()) {    // just log when debugging
        psolver->print();
        psolver->appendResultToSheet( instance, LOG_FILE );
    }

    // save context of the solver
    if (argvMap.find( ARGV_CUSTOM_OUTPUT ) != argvMap.end()) {
        input.customOutputFileName = argvMap[ARGV_CUSTOM_OUTPUT];

        // TODO


    }

    // do cleaning
    if (psolver != NULL) {
        delete psolver;
    }
}

void readScenario( const std::string &scenarioFileName, NurseRostering &input )
{
    NurseRostering::Scenario &scenario = input.scenario;
    char c;
    char buf[MAX_BUF_SIZE];
    ifstream ifs( scenarioFileName );

    ifs.getline( buf, MAX_BUF_LEN );        // SCENARIO = nXXXwX

    ifs.getline( buf, MAX_BUF_LEN );        // empty line
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // WEEKS =
    ifs >> scenario.weekNum;
    ifs.getline( buf, MAX_BUF_LEN );        // clear line

    ifs.getline( buf, MAX_BUF_LEN );        // empty line
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // SKILLS =
    ifs >> scenario.skillTypeNum;
    scenario.skillNames.resize( scenario.skillTypeNum );
    for (int i = 0; i < scenario.skillTypeNum; i++) {
        ifs >> scenario.skillNames[i];
        input.skillMap[scenario.skillNames[i]] = i;
    }
    ifs.getline( buf, MAX_BUF_LEN );        // clear line

    ifs.getline( buf, MAX_BUF_LEN );        // empty line
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // SHIFT_TYPES =
    ifs >> scenario.shiftTypeNum;
    scenario.shifts.resize( scenario.shiftTypeNum );
    for (int i = 0; i < scenario.shiftTypeNum; i++) {
        NurseRostering::Scenario::Shift &shift = scenario.shifts[i];
        ifs >> shift.name >> c    // name (
            >> shift.minConsecutiveShiftNum >> c   // XX,
            >> shift.maxConsecutiveShiftNum >> c;  // XX)
        input.shiftMap[shift.name] = i;
    }
    ifs.getline( buf, MAX_BUF_LEN );        // clear line

    ifs.getline( buf, MAX_BUF_LEN );        // empty line
    ifs.getline( buf, MAX_BUF_LEN );        // FORBIDDEN_SHIFT_TYPES_SUCCESSIONS
    for (int i = 0; i < scenario.shiftTypeNum; i++) {
        NurseRostering::Scenario::Shift &shift = scenario.shifts[i];
        string shiftName, nextShiftName;
        int succesionNum;
        ifs >> shiftName >> succesionNum;
        shift.illegalNextShifts = vector<bool>( scenario.shiftTypeNum, false );
        for (int j = 0; j < succesionNum; j++) {
            ifs >> nextShiftName;
            shift.illegalNextShifts[input.shiftMap[nextShiftName]] = true;
        }
    }
    ifs.getline( buf, MAX_BUF_LEN );        // clear line

    ifs.getline( buf, MAX_BUF_LEN );        // empty line
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // CONTRACTS =
    int contractNum;
    ifs >> contractNum;
    scenario.contracts.resize( contractNum );
    for (int i = 0; i < contractNum; i++) {
        NurseRostering::Scenario::Contract &contract = scenario.contracts[i];
        ifs >> contract.name >> c                               // name (
            >> contract.minShiftNum >> c                        // XX,
            >> contract.maxShiftNum >> c >> c                   // XX) (
            >> contract.minConsecutiveWorkingDayNum >> c        // XX,
            >> contract.maxConsecutiveWorkingDayNum >> c >> c   // XX) (
            >> contract.minConsecutiveDayoffNum >> c            // XX,
            >> contract.maxConsecutiveDayoffNum >> c            // )
            >> contract.maxWorkingWeekendNum
            >> contract.completeWeekend;
        input.contractMap[contract.name] = i;
    }
    ifs.getline( buf, MAX_BUF_LEN );        // clear line

    ifs.getline( buf, MAX_BUF_LEN );        // empty line
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // NURSES =
    ifs >> scenario.nurseNum;
    scenario.nurses.resize( scenario.nurseNum );
    for (int i = 0; i < scenario.nurseNum; i++) {
        NurseRostering::Scenario::Nurse &nurse = scenario.nurses[i];
        int skillNum;
        string contractName, skillName;
        ifs >> nurse.name >> contractName >> skillNum;
        input.nurseMap[nurse.name] = i;
        nurse.contract = input.contractMap[contractName];
        nurse.skills.resize( skillNum );
        for (int j = 0; j < skillNum; j++) {
            ifs >> skillName;
            nurse.skills[j] = input.skillMap[skillName];
        }
    }

    ifs.close();
}

void readHistory( const std::string &historyFileName, NurseRostering &input )
{
    NurseRostering::History &history = input.history;
    char buf[MAX_BUF_SIZE];
    ifstream ifs( historyFileName );

    ifs.getline( buf, MAX_BUF_LEN );    // HISTORY
    ifs >> input.weekCount;             // X
    ifs.getline( buf, MAX_BUF_LEN );    //  nXXXwX
    ifs.getline( buf, MAX_BUF_LEN );    // empty line
    ifs.getline( buf, MAX_BUF_LEN );    // NURSE_HISTORY

    history.resize( input.scenario.nurses.size() );
    for (int i = history.size(); i > 0; i--) {
        string nurseName, lastShiftName;
        ifs >> nurseName;
        NurseRostering::NurseHistory &nurseHistory = history[input.nurseMap[nurseName]];
        ifs >> nurseHistory.shiftNum >> nurseHistory.workingWeekendNum
            >> lastShiftName >> nurseHistory.consecutiveShiftNum
            >> nurseHistory.consecutiveWorkingDayNum >> nurseHistory.consecutiveDayoffNum;
        nurseHistory.lastShift = input.shiftMap[lastShiftName];
    }

    ifs.close();
}

void readWeekData( const std::string &weekDataFileName, NurseRostering &input )
{
    NurseRostering::WeekData &weekdata = input.weekData;
    weekdata.minNurseNums = vector< vector< vector<int> > >( NurseRostering::WEEKDAY_NUM, vector< vector<int> >( input.scenario.shifts.size(), vector<int>( input.scenario.skillNames.size() ) ) );
    weekdata.optNurseNums = vector< vector< vector<int> > >( NurseRostering::WEEKDAY_NUM, vector< vector<int> >( input.scenario.shifts.size(), vector<int>( input.scenario.skillNames.size() ) ) );
    weekdata.shiftOffs = vector< vector< vector<bool> > >( NurseRostering::WEEKDAY_NUM, vector< vector<bool> >( input.scenario.shifts.size(), vector<bool>( input.scenario.nurses.size(), false ) ) );
    char c;
    char buf[MAX_BUF_SIZE];
    ifstream ifs( weekDataFileName );

    ifs.getline( buf, MAX_BUF_LEN );    // WEEK_DATA
    ifs.getline( buf, MAX_BUF_LEN );    // nXXXwX
    ifs.getline( buf, MAX_BUF_LEN );    // empty line

    ifs.getline( buf, MAX_BUF_LEN );    // REQUIREMENTS
    while (true) {
        string shiftName, skillName;
        ifs >> shiftName >> skillName;
        if (skillName == "=") {         // SHIFT_OFF_REQUESTS =
            break;
        }
        NurseRostering::ShiftID shift = input.shiftMap[shiftName];
        NurseRostering::SkillID skill = input.skillMap[skillName];
        for (int weekday = 0; weekday < 7; weekday++) {
            ifs >> c >> weekdata.minNurseNums[weekday][shift][skill]
                >> c >> weekdata.optNurseNums[weekday][shift][skill] >> c;
        }
    }

    int shiftOffNum;
    ifs >> shiftOffNum;
    map<string, int> weekdayMap;
    weekdayMap["Mon"] = 0;
    weekdayMap["Tue"] = 1;
    weekdayMap["Wed"] = 2;
    weekdayMap["Thu"] = 3;
    weekdayMap["Fri"] = 4;
    weekdayMap["Sat"] = 5;
    weekdayMap["Sun"] = 6;
    for (int i = 0; i < shiftOffNum; i++) {
        string nurseName, shiftName, weekdayName;
        ifs >> nurseName >> shiftName >> weekdayName;
        int weekday = weekdayMap[weekdayName];
        NurseRostering::ShiftID shift = input.shiftMap[shiftName];
        NurseRostering::NurseID nurse = input.nurseMap[nurseName];
        if (shift == NurseRostering::Scenario::Shift::ID_ANY) {
            for (int s = input.scenario.shifts.size() - 1; s >= 0; s--) {
                weekdata.shiftOffs[weekday][s][nurse] = true;
            }
        } else {
            weekdata.shiftOffs[weekday][shift][nurse] = true;
        }
    }

    ifs.close();
}

NurseRostering::Solver* readCustomInput( const std::string &customInputFileName )
{
    return NULL;
}
