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
        if (argvMap.find( ARGV_SCENARIO ) != argvMap.end()) {
            readScenario( argvMap[ARGV_SCENARIO], input );
        } else {
#ifdef INRC2_DEBUG
            cerr << "missing obligate argument(scenario)" << endl;
#endif
            return;
        }
        if (argvMap.find( ARGV_CUSTOM_INPUT ) != argvMap.end()) {
            readCustomInput( argvMap[ARGV_CUSTOM_INPUT], input );
        } else if (argvMap.find( ARGV_HISTORY ) != argvMap.end()) {
            readHistory( argvMap[ARGV_HISTORY], input );
        } else {
#ifdef INRC2_DEBUG
            cerr << "missing obligate argument(history)" << endl;
#endif
            return;
        }
        if (argvMap.find( ARGV_WEEKDATA ) != argvMap.end()) {
            readWeekData( argvMap[ARGV_WEEKDATA], input );
        } else {
#ifdef INRC2_DEBUG
            cerr << "missing obligate argument(week data)" << endl;
#endif
            return;
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
#ifdef INRC2_DEBUG
            cerr << "missing obligate argument(solution file name)" << endl;
#endif
            return;
        }
        if (argvMap.find( ARGV_CUSTOM_OUTPUT ) != argvMap.end()) {
            writeCustomOutput( argvMap[ARGV_CUSTOM_OUTPUT], solver );
        }

#ifdef INRC2_DEBUG
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
#endif
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
                >> contract.minConsecutiveDayNum >> c        // XX,
                >> contract.maxConsecutiveDayNum >> c >> c   // XX) (
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
        ifs >> history.pastWeekCount;         // X
        history.currentWeek = history.pastWeekCount + 1;
        ifs.getline( buf, MAX_BUF_LEN );    //  nXXXwX
        ifs.getline( buf, MAX_BUF_LEN );    // empty line
        ifs.getline( buf, MAX_BUF_LEN );    // NURSE_HISTORY

        history.totalAssignNums.resize( input.scenario.nurseNum );
        history.totalWorkingWeekendNums.resize( input.scenario.nurseNum );
        history.lastShifts.resize( input.scenario.nurseNum );
        history.consecutiveShiftNums.resize( input.scenario.nurseNum );
        history.consecutiveDayNums.resize( input.scenario.nurseNum );
        history.consecutiveDayoffNums.resize( input.scenario.nurseNum );
        for (int i = input.scenario.nurseNum; i > 0; --i) {
            string nurseName, lastShiftName;
            ifs >> nurseName;
            NurseRostering::NurseID nurse = input.names.nurseMap[nurseName];
            ifs >> history.totalAssignNums[nurse] >> history.totalWorkingWeekendNums[nurse]
                >> lastShiftName >> history.consecutiveShiftNums[nurse]
                >> history.consecutiveDayNums[nurse] >> history.consecutiveDayoffNums[nurse];
            history.lastShifts[nurse] = input.names.shiftMap[lastShiftName];
        }

        ifs.close();
    }

    void readWeekData( const std::string &weekDataFileName, NurseRostering &input )
    {
        NurseRostering::WeekData &weekdata = input.weekData;
        weekdata.minNurseNums = vector< vector< vector<int> > >( NurseRostering::Weekday::SIZE, vector< vector<int> >( input.scenario.shifts.size(), vector<int>( input.scenario.skillTypeNum ) ) );
        weekdata.optNurseNums = vector< vector< vector<int> > >( NurseRostering::Weekday::SIZE, vector< vector<int> >( input.scenario.shifts.size(), vector<int>( input.scenario.skillTypeNum ) ) );
        weekdata.shiftOffs = vector< vector< vector<bool> > >( NurseRostering::Weekday::SIZE, vector< vector<bool> >( input.scenario.shifts.size(), vector<bool>( input.scenario.nurseNum, false ) ) );
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
            for (int weekday = NurseRostering::Weekday::Mon; weekday < NurseRostering::Weekday::SIZE; ++weekday) {
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
        ifstream ifs( customInputFileName, ios::binary );

        const int nurseNum = input.scenario.nurseNum;
        int *totalAssignNums = new int[nurseNum];
        int *totalWorkingWeekendNums = new int[nurseNum];
        NurseRostering::ShiftID *lastShifts = new NurseRostering::ShiftID[nurseNum];
        int *consecutiveShiftNums = new int[nurseNum];
        int *consecutiveDayNums = new int[nurseNum];
        int *consecutiveDayoffNums = new int[nurseNum];

        ifs.read( reinterpret_cast<char *>(&input.history.pastWeekCount), sizeof( input.history.pastWeekCount ) );
        ifs.read( reinterpret_cast<char *>(&input.history.currentWeek), sizeof( input.history.currentWeek ) );
        ifs.read( reinterpret_cast<char *>(totalAssignNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(totalWorkingWeekendNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(lastShifts), nurseNum * sizeof( NurseRostering::ShiftID ) );
        ifs.read( reinterpret_cast<char *>(consecutiveShiftNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(consecutiveDayNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(consecutiveDayoffNums), nurseNum * sizeof( int ) );

        input.history.totalAssignNums = vector<int>( totalAssignNums, totalAssignNums + nurseNum );
        input.history.totalWorkingWeekendNums = vector<int>( totalWorkingWeekendNums, totalWorkingWeekendNums + nurseNum );
        input.history.lastShifts = vector<int>( lastShifts, lastShifts + nurseNum );
        input.history.consecutiveShiftNums = vector<int>( consecutiveShiftNums, consecutiveShiftNums + nurseNum );
        input.history.consecutiveDayNums = vector<int>( consecutiveDayNums, consecutiveDayNums + nurseNum );
        input.history.consecutiveDayoffNums = vector<int>( consecutiveDayoffNums, consecutiveDayoffNums + nurseNum );

        delete[] totalAssignNums;
        delete[] totalWorkingWeekendNums;
        delete[] lastShifts;
        delete[] consecutiveShiftNums;
        delete[] consecutiveDayNums;
        delete[] consecutiveDayoffNums;

        ifs.close();
    }

    void writeSolution( const std::string &solutionFileName, const NurseRostering::Solver &solver )
    {
        const NurseRostering::Names &names( solver.problem.names );
        const NurseRostering::Assign &assign( solver.getOptima().assign );
        ofstream ofs( solutionFileName );

        ofs << "SOLUTION" << endl;
        ofs << solver.problem.history.pastWeekCount << ' '
            << solver.problem.names.scenarioName << endl << endl;
        int totalAssign = 0;
        ostringstream oss;
        for (NurseRostering::NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
            for (int weekday = NurseRostering::Weekday::Mon; weekday < NurseRostering::Weekday::SIZE; ++weekday) {
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

    void writeCustomOutput( const std::string &customOutputFileName, const NurseRostering::Solver &solver )
    {
        NurseRostering::History history( solver.genHistory() );

        ofstream ofs( customOutputFileName, ios::binary );
        ofs.write( reinterpret_cast<const char *>(&history.pastWeekCount), sizeof( history.pastWeekCount ) );
        ofs.write( reinterpret_cast<const char *>(&history.currentWeek), sizeof( history.currentWeek ) );
        ofs.write( reinterpret_cast<const char *>(history.totalAssignNums.data()), history.totalAssignNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.totalWorkingWeekendNums.data()), history.totalWorkingWeekendNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.lastShifts.data()), history.lastShifts.size() * sizeof( NurseRostering::ShiftID ) );
        ofs.write( reinterpret_cast<const char *>(history.consecutiveShiftNums.data()), history.consecutiveShiftNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.consecutiveDayNums.data()), history.consecutiveDayNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.consecutiveDayoffNums.data()), history.consecutiveDayoffNums.size() * sizeof( int ) );
        ofs.close();
    }
}