#include "INRC2.h"


using namespace std;


namespace INRC2
{
    const std::string LOG_FILE_NAME( "log.csv" );

    const std::string ARGV_ID( "id" );
    const std::string ARGV_SCENARIO( "sce" );
    const std::string ARGV_HISTORY( "his" );
    const std::string ARGV_WEEKDATA( "week" );
    const std::string ARGV_SOLUTION( "sol" );
    const std::string ARGV_CUSTOM_INPUT( "cusIn" );
    const std::string ARGV_CUSTOM_OUTPUT( "cusOut" );
    const std::string ARGV_RANDOM_SEED( "rand" );
    const std::string ARGV_TIME( "timeout" );  // in seconds
    const std::string ARGV_CONFIG( "config" );
    const std::string ARGV_HELP( "help" );

    const std::string weekdayNames[NurseRostering::Weekday::SIZE] = {
        "HIS", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "NEXT_WEEK"
    };
    const std::map<std::string, int> weekdayMap = {
        { weekdayNames[NurseRostering::Weekday::Mon], NurseRostering::Weekday::Mon },
        { weekdayNames[NurseRostering::Weekday::Tue], NurseRostering::Weekday::Tue },
        { weekdayNames[NurseRostering::Weekday::Wed], NurseRostering::Weekday::Wed },
        { weekdayNames[NurseRostering::Weekday::Thu], NurseRostering::Weekday::Thu },
        { weekdayNames[NurseRostering::Weekday::Fri], NurseRostering::Weekday::Fri },
        { weekdayNames[NurseRostering::Weekday::Sat], NurseRostering::Weekday::Sat },
        { weekdayNames[NurseRostering::Weekday::Sun], NurseRostering::Weekday::Sun }
    };


    void help()
    {
        cout <<
            "command line arguments ([XXX] means XXX is optional) :\n"
            " [id]      - identifier of the run which will be recorded in log file.\n"
            " sce       - scenario file path.\n"
            " his       - history file path.\n"
            " week      - weekdata file path.\n"
            " sol       - solution file path.\n"
            " [cusIn]   - custom input file path.\n"
            " [cusOut]   - custom output file path.\n"
            " [rand]    - rand seed for the solver.\n"
            " [timeout] - max running time of the solver.\n"
            " [config]  - specifies algorithm select and argument settings.\n"
            "             format: cci;d;d,d,d,d;d,d,d,d\n"
            "               c for char, d for real number, comma is used to separate numbers.\n"
            "               the first char can be 'g'(for greedy init) or 'e'(for exact init).\n"
            "               the second char can be 'w'(Random Walk), 'i'(Iterative Local Search),\n"
            "               'p'(TS Possibility), 'l'(TS Loop), 'r'(TS Rand), 's'(Swap Chain).\n"
            "               i for a non-negative integer corresponding to Solution::ModeSeq.\n"
            "               first real number for coefficient of max no improve count.\n"
            "               following 4 real numbers are coefficients for TableSize, NurseNum,\n"
            "               next real number is the coefficient of no improve count.\n"
            "               WeekdayNum and ShiftNum used in day tabu tenure setting,\n"
            "               non-positive number means there is no relation with that feature. while\n"
            "               next 4 numbers are used in shift tabu tenure setting with same meaning.\n"
            "             example: gt2;1.5;0,0.5,0,0;0,0.8,0,0 or gt3;0.8;0.1,0,0,0;0.1,0,0,0\n"
            << endl;
    }

    int run( int argc, char *argv[] )
    {
        clock_t startTime = clock();

        // handle command line arguments
        map<string, string> argvMap;
        for (int i = 1; i < argc; i += 2) {
            argvMap[argv[i] + 2] = argv[i + 1]; // (argv[i] + 2) to skip "--" before argument
        }

        // print help
        if (argvMap.find( ARGV_HELP ) != argvMap.end()) {
            help();
        }

        // read input
        NurseRostering input;
        // load scenario first, for week data may use information in it
        if (!readScenario( argvMap[ARGV_SCENARIO], input )) {
            return -1;
        }

        // load weekdata
        if (!readWeekData( argvMap[ARGV_WEEKDATA], input )) {
            return -1;
        }

        // load history last for it will initialize some assist data depending on scenario and weekdata
        if (argvMap.find( ARGV_CUSTOM_INPUT ) != argvMap.end()) {
            readCustomInput( argvMap[ARGV_CUSTOM_INPUT], input );
        } else if (argvMap.find( ARGV_HISTORY ) != argvMap.end()) {
            readHistory( argvMap[ARGV_HISTORY], input );
        } else {
            errorLog( "missing obligate argument(history)" );
            return -1;
        }

        // check solution file name
        if (argvMap.find( ARGV_SOLUTION ) == argvMap.end()) {
            errorLog( "missing obligate argument(solution file name)" );
            return -1;
        }

        // load random seed
        if (argvMap.find( ARGV_RANDOM_SEED ) != argvMap.end()) {
            istringstream iss( argvMap[ARGV_RANDOM_SEED] );
            iss >> input.randSeed;
        } else {
            input.randSeed = static_cast<int>(time( NULL ) + clock());
        }

        // load timeout
        if (argvMap.find( ARGV_TIME ) != argvMap.end()) {
            istringstream iss( argvMap[ARGV_TIME] );
            double timeout;
            iss >> timeout;
            input.timeout = static_cast<clock_t>(timeout * CLOCKS_PER_SEC)
                - NurseRostering::Solver::SAVE_SOLUTION_TIME;  // convert second to clock count
        } else {
            input.timeout = NurseRostering::MAX_RUNNING_TIME;
        }

        // start computation
        input.adjustRangeOfTotalAssignByWorkload();
        NurseRostering::TabuSolver solver( input, startTime );
        solver.init( parseConfig( argvMap[ARGV_CONFIG] ), argvMap[ARGV_ID] );
        solver.solve();

        // write output
        writeSolution( argvMap[ARGV_SOLUTION], solver );
        if (argvMap.find( ARGV_CUSTOM_OUTPUT ) != argvMap.end()) {
            writeCustomOutput( argvMap[ARGV_CUSTOM_OUTPUT], solver );
        }

#ifdef INRC2_DEBUG
        solver.check();
        solver.print();
#endif

#ifdef INRC2_LOG
        ostringstream oss;
        int historyFileNameIndex = argvMap[ARGV_HISTORY].find_last_of( "/\\" ) + 1;
        int weekdataFileNameIndex = argvMap[ARGV_WEEKDATA].find_last_of( "/\\" ) + 1;
        oss << input.names.scenarioName
            << '[' << argvMap[ARGV_HISTORY].substr( historyFileNameIndex ) << ']'
            << '[' << argvMap[ARGV_WEEKDATA].substr( weekdataFileNameIndex ) << ']';
        solver.record( LOG_FILE_NAME, oss.str() );
#endif

        return 0;
    }

    bool readScenario( const std::string &scenarioFileName, NurseRostering &input )
    {
        NurseRostering::Scenario &scenario = input.scenario;
        char c;
        char buf[MAX_BUF_SIZE];
        ifstream ifs( scenarioFileName );

        if (!ifs.is_open()) {
            errorLog( "fail to open scenario file : " + scenarioFileName );
            return false;
        }

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
        scenario.skillSize = scenario.skillTypeNum + NurseRostering::Scenario::Skill::ID_BEGIN;
        input.names.skillNames.resize( scenario.skillSize );
        for (NurseRostering::SkillID i = NurseRostering::Scenario::Skill::ID_BEGIN; i < scenario.skillSize; ++i) {
            ifs >> input.names.skillNames[i];
            input.names.skillMap[input.names.skillNames[i]] = i;
        }
        ifs.getline( buf, MAX_BUF_LEN );        // clear line

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN, '=' );   // SHIFT_TYPES =
        ifs >> scenario.shiftTypeNum;
        scenario.shiftSize = scenario.shiftTypeNum + NurseRostering::Scenario::Shift::ID_BEGIN;
        scenario.shifts.resize( scenario.shiftSize );
        input.names.shiftNames.resize( scenario.shiftSize );
        for (NurseRostering::ShiftID i = NurseRostering::Scenario::Shift::ID_BEGIN; i < scenario.shiftSize; ++i) {
            NurseRostering::Scenario::Shift &shift = scenario.shifts[i];
            ifs >> input.names.shiftNames[i] >> c    // name (
                >> shift.minConsecutiveShiftNum >> c   // XX,
                >> shift.maxConsecutiveShiftNum >> c;  // XX)
            input.names.shiftMap[input.names.shiftNames[i]] = i;
        }
        ifs.getline( buf, MAX_BUF_LEN );        // clear line

        ifs.getline( buf, MAX_BUF_LEN );        // empty line
        ifs.getline( buf, MAX_BUF_LEN );        // FORBIDDEN_SHIFT_TYPES_SUCCESSIONS
        scenario.shifts[NurseRostering::Scenario::Shift::ID_NONE].legalNextShifts = vector<bool>( scenario.shiftSize, true );
        scenario.shifts[NurseRostering::Scenario::Shift::ID_NONE].illegalNextShiftNum = 0;
        for (NurseRostering::ShiftID i = NurseRostering::Scenario::Shift::ID_BEGIN; i < scenario.shiftSize; ++i) {
            NurseRostering::Scenario::Shift &shift = scenario.shifts[i];
            string shiftName, nextShiftName;
            int succesionNum;
            ifs >> shiftName >> succesionNum;
            shift.illegalNextShiftNum = succesionNum;
            shift.legalNextShifts = vector<bool>( scenario.shiftSize, true );
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
        for (NurseRostering::ContractID i = 0; i < contractNum; ++i) {
            NurseRostering::Scenario::Contract &contract = scenario.contracts[i];
            ifs >> input.names.contractNames[i] >> c         // name (
                >> contract.minShiftNum >> c                 // XX,
                >> contract.maxShiftNum >> c >> c            // XX) (
                >> contract.minConsecutiveDayNum >> c        // XX,
                >> contract.maxConsecutiveDayNum >> c >> c   // XX) (
                >> contract.minConsecutiveDayoffNum >> c     // XX,
                >> contract.maxConsecutiveDayoffNum >> c     // )
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
        for (NurseRostering::NurseID i = 0; i < scenario.nurseNum; ++i) {
            NurseRostering::Scenario::Nurse &nurse = scenario.nurses[i];
            string contractName, skillName;
            ifs >> input.names.nurseNames[i] >> contractName >> nurse.skillNum;
            input.names.nurseMap[input.names.nurseNames[i]] = i;
            nurse.contract = input.names.contractMap[contractName];
            nurse.skills = vector<bool>( scenario.skillSize, false );
            nurse.skills[NurseRostering::Scenario::Skill::ID_NONE] = true;
            for (int j = 0; j < nurse.skillNum; ++j) {
                ifs >> skillName;
                nurse.skills[input.names.skillMap[skillName]] = true;
            }
        }

        ifs.close();
        return true;
    }

    bool readHistory( const std::string &historyFileName, NurseRostering &input )
    {
        NurseRostering::History &history = input.history;
        char buf[MAX_BUF_SIZE];
        ifstream ifs( historyFileName );

        if (!ifs.is_open()) {
            errorLog( "fail to open history file : " + historyFileName );
            return false;
        }

        history.accObjValue = 0;
        ifs.getline( buf, MAX_BUF_LEN );    // HISTORY
        ifs >> history.pastWeekCount;         // X
        history.currentWeek = history.pastWeekCount + 1;
        history.restWeekCount = input.scenario.totalWeekNum - history.pastWeekCount;
        ifs.getline( buf, MAX_BUF_LEN );    //  nXXXwX
        ifs.getline( buf, MAX_BUF_LEN );    // empty line
        ifs.getline( buf, MAX_BUF_LEN );    // NURSE_HISTORY

        history.totalAssignNums.resize( input.scenario.nurseNum );
        history.totalWorkingWeekendNums.resize( input.scenario.nurseNum );
        history.lastShifts.resize( input.scenario.nurseNum );
        history.consecutiveShiftNums.resize( input.scenario.nurseNum );
        history.consecutiveDayNums.resize( input.scenario.nurseNum );
        history.consecutiveDayoffNums.resize( input.scenario.nurseNum );
        for (int i = 0; i < input.scenario.nurseNum; ++i) {
            string nurseName, lastShiftName;
            ifs >> nurseName;
            NurseRostering::NurseID nurse = input.names.nurseMap[nurseName];
            ifs >> history.totalAssignNums[nurse] >> history.totalWorkingWeekendNums[nurse]
                >> lastShiftName >> history.consecutiveShiftNums[nurse]
                >> history.consecutiveDayNums[nurse] >> history.consecutiveDayoffNums[nurse];
            history.lastShifts[nurse] = input.names.shiftMap[lastShiftName];
            if (history.pastWeekCount == 0) {   // clear total assign if it is the first week
                history.totalAssignNums[nurse] = 0;
                history.totalWorkingWeekendNums[nurse] = 0;
            }
        }

        ifs.close();
        return true;
    }

    bool readWeekData( const std::string &weekDataFileName, NurseRostering &input )
    {
        NurseRostering::WeekData &weekdata = input.weekData;
        weekdata.minNurseNums = vector< vector< vector<int> > >( NurseRostering::Weekday::SIZE,
            vector< vector<int> >( input.scenario.shiftSize, vector<int>( input.scenario.skillSize ) ) );
        weekdata.optNurseNums = vector< vector< vector<int> > >( NurseRostering::Weekday::SIZE,
            vector< vector<int> >( input.scenario.shiftSize, vector<int>( input.scenario.skillSize ) ) );
        weekdata.shiftOffs = vector< vector< vector<bool> > >( NurseRostering::Weekday::SIZE,
            vector< vector<bool> >( input.scenario.shiftSize, vector<bool>( input.scenario.nurseNum, false ) ) );
        char c;
        char buf[MAX_BUF_SIZE];
        ifstream ifs( weekDataFileName );

        if (!ifs.is_open()) {
            errorLog( "fail to open weekdata file : " + weekDataFileName );
            return false;
        }

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
            for (int weekday = NurseRostering::Weekday::Mon;
                weekday <= NurseRostering::Weekday::Sun; ++weekday) {
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
                for (int s = NurseRostering::Scenario::Shift::ID_BEGIN;
                    s < input.scenario.shiftSize; ++s) {
                    weekdata.shiftOffs[weekday][s][nurse] = true;
                }
            } else {
                weekdata.shiftOffs[weekday][shift][nurse] = true;
            }
        }

        ifs.close();
        return true;
    }

    bool readCustomInput( const std::string &customInputFileName, NurseRostering &input )
    {
        NurseRostering::History &history = input.history;
        ifstream ifs( customInputFileName, ios::binary );

        if (!ifs.is_open()) {
            errorLog( "fail to open custom input file." );
            return false;
        }

        const int nurseNum = input.scenario.nurseNum;
        int *totalAssignNums = new int[nurseNum];
        int *totalWorkingWeekendNums = new int[nurseNum];
        NurseRostering::ShiftID *lastShifts = new NurseRostering::ShiftID[nurseNum];
        int *consecutiveShiftNums = new int[nurseNum];
        int *consecutiveDayNums = new int[nurseNum];
        int *consecutiveDayoffNums = new int[nurseNum];

        ifs.read( reinterpret_cast<char *>(&history.accObjValue), sizeof( history.accObjValue ) );
        ifs.read( reinterpret_cast<char *>(&history.pastWeekCount), sizeof( history.pastWeekCount ) );
        ifs.read( reinterpret_cast<char *>(&history.currentWeek), sizeof( history.currentWeek ) );
        ifs.read( reinterpret_cast<char *>(totalAssignNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(totalWorkingWeekendNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(lastShifts), nurseNum * sizeof( NurseRostering::ShiftID ) );
        ifs.read( reinterpret_cast<char *>(consecutiveShiftNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(consecutiveDayNums), nurseNum * sizeof( int ) );
        ifs.read( reinterpret_cast<char *>(consecutiveDayoffNums), nurseNum * sizeof( int ) );

        history.totalAssignNums = vector<int>( totalAssignNums, totalAssignNums + nurseNum );
        history.totalWorkingWeekendNums = vector<int>( totalWorkingWeekendNums, totalWorkingWeekendNums + nurseNum );
        history.lastShifts = vector<int>( lastShifts, lastShifts + nurseNum );
        history.consecutiveShiftNums = vector<int>( consecutiveShiftNums, consecutiveShiftNums + nurseNum );
        history.consecutiveDayNums = vector<int>( consecutiveDayNums, consecutiveDayNums + nurseNum );
        history.consecutiveDayoffNums = vector<int>( consecutiveDayoffNums, consecutiveDayoffNums + nurseNum );

        delete[] totalAssignNums;
        delete[] totalWorkingWeekendNums;
        delete[] lastShifts;
        delete[] consecutiveShiftNums;
        delete[] consecutiveDayNums;
        delete[] consecutiveDayoffNums;

        ifs.close();
        return true;
    }

    bool writeSolution( const std::string &solutionFileName, const NurseRostering::Solver &solver )
    {
        const NurseRostering::Names &names( solver.problem.names );
        const NurseRostering::AssignTable &assign( solver.getOptima().getAssignTable() );
        ofstream ofs( solutionFileName );

        if (!ofs.is_open()) {
            errorLog( "fail to open solution file : " + solutionFileName );
            return false;
        }

        ofs << "SOLUTION" << endl;
        ofs << solver.problem.history.pastWeekCount << ' '
            << solver.problem.names.scenarioName << endl << endl;
        int totalAssign = 0;
        ostringstream oss;
        for (NurseRostering::NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
            for (int weekday = NurseRostering::Weekday::Mon; weekday <= NurseRostering::Weekday::Sun; ++weekday) {
                if (assign.isWorking( nurse, weekday )) {
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

        ofs.close();
        return true;
    }

    bool writeCustomOutput( const std::string &customOutputFileName, const NurseRostering::Solver &solver )
    {
        NurseRostering::History history( solver.genHistory() );

        ofstream ofs( customOutputFileName, ios::binary );

        if (!ofs.is_open()) {
            errorLog( "fail to open custom output file : " + customOutputFileName );
            return false;
        }

        ofs.write( reinterpret_cast<const char *>(&history.accObjValue), sizeof( history.accObjValue ) );
        ofs.write( reinterpret_cast<const char *>(&history.pastWeekCount), sizeof( history.pastWeekCount ) );
        ofs.write( reinterpret_cast<const char *>(&history.currentWeek), sizeof( history.currentWeek ) );
        ofs.write( reinterpret_cast<const char *>(history.totalAssignNums.data()), history.totalAssignNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.totalWorkingWeekendNums.data()), history.totalWorkingWeekendNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.lastShifts.data()), history.lastShifts.size() * sizeof( NurseRostering::ShiftID ) );
        ofs.write( reinterpret_cast<const char *>(history.consecutiveShiftNums.data()), history.consecutiveShiftNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.consecutiveDayNums.data()), history.consecutiveDayNums.size() * sizeof( int ) );
        ofs.write( reinterpret_cast<const char *>(history.consecutiveDayoffNums.data()), history.consecutiveDayoffNums.size() * sizeof( int ) );

        ofs.close();
        return true;
    }

    NurseRostering::Solver::Config parseConfig( const std::string &configString )
    {
        NurseRostering::Solver::Config config;
        istringstream iss( configString );
        char c;
        int modeSeq;

        iss >> c;
        if (c == 'g') {
            config.initAlgorithm = NurseRostering::Solver::InitAlgorithm::Greedy;
        } else if (c == 'e') {
            config.initAlgorithm = NurseRostering::Solver::InitAlgorithm::Exact;
        } else {
            return config;
        }

        iss >> c;
        if (c == 'w') {
            config.solveAlgorithm = NurseRostering::Solver::SolveAlgorithm::RandomWalk;
        } else if (c == 'i') {
            config.solveAlgorithm = NurseRostering::Solver::SolveAlgorithm::IterativeLocalSearch;
        } else if (c == 'p') {
            config.solveAlgorithm = NurseRostering::Solver::SolveAlgorithm::TabuSearch_Possibility;
        } else if (c == 'l') {
            config.solveAlgorithm = NurseRostering::Solver::SolveAlgorithm::TabuSearch_Loop;
        } else if (c == 'r') {
            config.solveAlgorithm = NurseRostering::Solver::SolveAlgorithm::TabuSearch_Rand;
        } else if (c == 's') {
            config.solveAlgorithm = NurseRostering::Solver::SolveAlgorithm::SwapChainSearch;
        } else {
            return config;
        }

        iss >> modeSeq;
        if ((modeSeq >= 0) && (modeSeq < NurseRostering::Solution::ModeSeq::SIZE)) {
            config.modeSeq = static_cast<NurseRostering::Solution::ModeSeq>(modeSeq);
        }

        iss >> c;
        iss >> config.maxNoImproveCoefficient;

        iss >> c >> config.dayTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::TableSize]
            >> c >> config.dayTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::NurseNum]
            >> c >> config.dayTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::DayNum]
            >> c >> config.dayTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::ShiftNum]
            >> c >> config.shiftTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::TableSize]
            >> c >> config.shiftTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::NurseNum]
            >> c >> config.shiftTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::DayNum]
            >> c >> config.shiftTabuCoefficient[NurseRostering::Solver::TabuTenureCoefficientIndex::ShiftNum];

        return config;
    }

}
