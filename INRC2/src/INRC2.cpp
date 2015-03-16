#include "INRC2.h"


using namespace std;


namespace INRC2
{
    void run( int argc, char *argv[] )
    {
        clock_t startTime = clock();

        // handle command line arguments
        map<string, string> argvMap;
        for (int i = 1; i < argc; i += 2) {
            argvMap[argv[i] + 2] = argv[i + 1]; // (argv[i] + 2) to skip "--" before argument
        }

        // read input
        NurseRostering input;
        // recover scenario first, for week data may use information in it
        // if there is custom input, the context can be recovered more efficiently
        if (argvMap.find( ARGV_CUSTOM_INPUT ) != argvMap.end()) {
            readCustomInput( argvMap[ARGV_CUSTOM_INPUT], input );
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
        if (argvMap.find( ARGV_RANDOM_SEED ) != argvMap.end()) {
            istringstream iss( argvMap[ARGV_RANDOM_SEED] );
            iss >> input.randSeed;
        } else {
            input.randSeed = static_cast<int>(time( NULL ) + clock());
        }
        if (argvMap.find( ARGV_TIME ) != argvMap.end()) {
            istringstream iss( argvMap[ARGV_TIME] );
            double timeout;
            iss >> timeout;
            input.timeout = static_cast<int>(timeout * 1000);  // convert second to millisecond
        } else {
            input.timeout = NurseRostering::MAX_RUNNING_TIME;
        }

        // start computation
        NurseRostering::TabuSolver solver( input, startTime );
        solver.init();
        solver.solve();

        // write output
        if (argvMap.find( ARGV_SOLUTION ) != argvMap.end()) {
            writeSolution( argvMap[ARGV_SOLUTION], solver );
        } else {
            cerr << "missing obligate argument(solution file name)" << endl;
        }
        if (argvMap.find( ARGV_CUSTOM_OUTPUT ) != argvMap.end()) {
            writeCustomOutput( argvMap[ARGV_CUSTOM_OUTPUT] );
        }

        //[!] just check and log when debugging
        if (!solver.check()) {
            cerr << "logic error in optima solution." << endl;
        }
        solver.print();
        ostringstream oss;
        int historyFileNameIndex = argvMap[ARGV_HISTORY].find_last_of( "/\\" ) + 1;
        int weekdataFileNameIndex = argvMap[ARGV_WEEKDATA].find_last_of( "/\\" ) + 1;
        oss << input.names.scenarioName
            << '[' << argvMap[ARGV_HISTORY].substr( historyFileNameIndex ) << ']'
            << '[' << argvMap[ARGV_WEEKDATA].substr( weekdataFileNameIndex ) << ']';
        solver.record( LOG_FILE, oss.str() );
    }

    void readScenario( const std::string &scenarioFileName, NurseRostering &input )
    {
        NurseRostering::Scenario &scenario = input.scenario;
        char c;
        char buf[MAX_BUF_SIZE];
        ifstream ifs( scenarioFileName );

        ifs.getline( buf, MAX_BUF_LEN, '=' );   // SCENARIO =
        ifs >> input.names.scenarioName;        //  nXXXwX

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN, '=' );   // WEEKS =
        ifs >> scenario.totalWeekNum;
        scenario.maxWeekCount = scenario.totalWeekNum - 1;
        ifs.getline( buf, MAX_BUF_LEN );        // clear line

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN, '=' );   // SKILLS =
        ifs >> scenario.skillTypeNum;
        input.names.skillNames.resize( scenario.skillTypeNum );
        for (int i = 0; i < scenario.skillTypeNum; ++i) {
            ifs >> input.names.skillNames[i];
            input.names.skillMap[input.names.skillNames[i]] = i;
        }
        ifs.getline( buf, MAX_BUF_LEN );        // clear line

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN, '=' );   // SHIFT_TYPES =
        ifs >> scenario.shiftTypeNum;
        scenario.shifts.resize( scenario.shiftTypeNum );
        input.names.shiftNames.resize( scenario.shiftTypeNum );
        for (int i = 0; i < scenario.shiftTypeNum; ++i) {
            NurseRostering::Scenario::Shift &shift = scenario.shifts[i];
            ifs >> input.names.shiftNames[i] >> c    // name (
                >> shift.minConsecutiveShiftNum >> c   // XX,
                >> shift.maxConsecutiveShiftNum >> c;  // XX)
            input.names.shiftMap[input.names.shiftNames[i]] = i;
        }
        ifs.getline( buf, MAX_BUF_LEN );        // clear line

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN );        // FORBIDDEN_SHIFT_TYPES_SUCCESSIONS
        for (int i = 0; i < scenario.shiftTypeNum; ++i) {
            NurseRostering::Scenario::Shift &shift = scenario.shifts[i];
            string shiftName, nextShiftName;
            int succesionNum;
            ifs >> shiftName >> succesionNum;
            shift.legalNextShifts = vector<bool>( scenario.shiftTypeNum, true );
            for (int j = 0; j < succesionNum; ++j) {
                ifs >> nextShiftName;
                shift.legalNextShifts[input.names.shiftMap[nextShiftName]] = false;
            }
        }
        ifs.getline( buf, MAX_BUF_LEN );        // clear line

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN, '=' );   // CONTRACTS =
        int contractNum;
        ifs >> contractNum;
        scenario.contracts.resize( contractNum );
        input.names.contractNames.resize( contractNum );
        for (int i = 0; i < contractNum; ++i) {
            NurseRostering::Scenario::Contract &contract = scenario.contracts[i];
            ifs >> input.names.contractNames[i] >> c            // name (
                >> contract.minShiftNum >> c                        // XX,
                >> contract.maxShiftNum >> c >> c                   // XX) (
                >> contract.minConsecutiveWorkingDayNum >> c        // XX,
                >> contract.maxConsecutiveWorkingDayNum >> c >> c   // XX) (
                >> contract.minConsecutiveDayoffNum >> c            // XX,
                >> contract.maxConsecutiveDayoffNum >> c            // )
                >> contract.maxWorkingWeekendNum
                >> contract.completeWeekend;
            input.names.contractMap[input.names.contractNames[i]] = i;
        }
        ifs.getline( buf, MAX_BUF_LEN );        // clear line

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN, '=' );   // NURSES =
        ifs >> scenario.nurseNum;
        scenario.nurses.resize( scenario.nurseNum );
        input.names.nurseNames.resize( scenario.nurseNum );
        for (int i = 0; i < scenario.nurseNum; ++i) {
            NurseRostering::Scenario::Nurse &nurse = scenario.nurses[i];
            int skillNum;
            string contractName, skillName;
            ifs >> input.names.nurseNames[i] >> contractName >> skillNum;
            input.names.nurseMap[input.names.nurseNames[i]] = i;
            nurse.contract = input.names.contractMap[contractName];
            nurse.skills.resize( skillNum );
            for (int j = 0; j < skillNum; ++j) {
                ifs >> skillName;
                nurse.skills[j] = input.names.skillMap[skillName];
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
        ifs >> input.pastWeekCount;         // X
        input.currentWeek = input.pastWeekCount + 1;
        ifs.getline( buf, MAX_BUF_LEN );    //  nXXXwX
        ifs.getline( buf, MAX_BUF_LEN );    // empty line
        ifs.getline( buf, MAX_BUF_LEN );    // NURSE_HISTORY

        history.shiftNum.resize( input.scenario.nurseNum );
        history.workingWeekendNum.resize( input.scenario.nurseNum );
        history.lastShift.resize( input.scenario.nurseNum );
        history.consecutiveShiftNum.resize( input.scenario.nurseNum );
        history.consecutiveWorkingDayNum.resize( input.scenario.nurseNum );
        history.consecutiveDayoffNum.resize( input.scenario.nurseNum );
        for (int i = input.scenario.nurseNum; i > 0; --i) {
            string nurseName, lastShiftName;
            ifs >> nurseName;
            NurseRostering::NurseID nurse = input.names.nurseMap[nurseName];
            ifs >> history.shiftNum[nurse] >> history.workingWeekendNum[nurse]
                >> lastShiftName >> history.consecutiveShiftNum[nurse]
                >> history.consecutiveWorkingDayNum[nurse] >> history.consecutiveDayoffNum[nurse];
            history.lastShift[nurse] = input.names.shiftMap[lastShiftName];
        }

        ifs.close();
    }

    void readWeekData( const std::string &weekDataFileName, NurseRostering &input )
    {
        NurseRostering::WeekData &weekdata = input.weekData;
        weekdata.minNurseNums = vector< vector< vector<int> > >( NurseRostering::Weekday::NUM, vector< vector<int> >( input.scenario.shifts.size(), vector<int>( input.scenario.skillTypeNum ) ) );
        weekdata.optNurseNums = vector< vector< vector<int> > >( NurseRostering::Weekday::NUM, vector< vector<int> >( input.scenario.shifts.size(), vector<int>( input.scenario.skillTypeNum ) ) );
        weekdata.shiftOffs = vector< vector< vector<bool> > >( NurseRostering::Weekday::NUM, vector< vector<bool> >( input.scenario.shifts.size(), vector<bool>( input.scenario.nurseNum, false ) ) );
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
            NurseRostering::ShiftID shift = input.names.shiftMap[shiftName];
            NurseRostering::SkillID skill = input.names.skillMap[skillName];
            for (int weekday = NurseRostering::Weekday::Mon; weekday < 7; ++weekday) {
                ifs >> c >> weekdata.minNurseNums[weekday][shift][skill]
                    >> c >> weekdata.optNurseNums[weekday][shift][skill] >> c;
            }
        }

        int shiftOffNum;
        ifs >> shiftOffNum;
        for (int i = 0; i < shiftOffNum; ++i) {
            string nurseName, shiftName, weekdayName;
            ifs >> nurseName >> shiftName >> weekdayName;
            int weekday = weekdayMap.at( weekdayName );
            NurseRostering::ShiftID shift = input.names.shiftMap[shiftName];
            NurseRostering::NurseID nurse = input.names.nurseMap[nurseName];
            if (shift == NurseRostering::Scenario::Shift::ID_ANY) {
                for (int s = input.scenario.shifts.size() - 1; s >= 0; --s) {
                    weekdata.shiftOffs[weekday][s][nurse] = true;
                }
            } else {
                weekdata.shiftOffs[weekday][shift][nurse] = true;
            }
        }

        ifs.close();
    }

    void readCustomInput( const std::string &customInputFileName, NurseRostering &input )
    {

    }

    void writeSolution( const std::string &solutionFileName, const NurseRostering::Solver &solver )
    {
        const NurseRostering::Names &names( solver.problem.names );
        const NurseRostering::Assign &assign( solver.getOptima().assign );
        ofstream ofs( solutionFileName );

        ofs << "SOLUTION" << endl;
        ofs << solver.problem.pastWeekCount << ' '
            << solver.problem.names.scenarioName << endl << endl;
        int totalAssign = 0;
        ostringstream oss;
        for (NurseRostering::NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
            for (int weekday = NurseRostering::Weekday::Mon; weekday < NurseRostering::Weekday::NUM; ++weekday) {
                if (assign[nurse][weekday].shift != NurseRostering::Scenario::Shift::ID_NONE) {
                    ++totalAssign;
                    oss << names.nurseNames[nurse] << ' '
                        << weekdayNames[weekday] << ' '
                        << names.shiftNames[assign[nurse][weekday].shift] << ' '
                        << names.skillNames[assign[nurse][weekday].skill]
                        << endl;
                }
            }
        }
        ofs << "ASSIGNMENTS = " << totalAssign << endl;
        ofs << oss.str();
    }

    void writeCustomOutput( const std::string &customOutputFileName )
    {

    }
}