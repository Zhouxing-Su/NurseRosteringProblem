#include "INRC2.h"


using namespace std;


int main( int argc, char **argv )
{
    // handle command line arguments
    map<string, string> argvMap;
    for (int i = 1; i < argc; i += 2) {
        argvMap[argv[i] + 2] = argv[i + 1];
    }

    NurseRostering::Solver::Input input;
    NurseRostering::Solver *psolver = NULL;
    if (argvMap.find( ARGV_SCENARIO ) != argvMap.end()) {
        readScenario( argvMap[ARGV_SCENARIO], input.scenario );
    }
    if (argvMap.find( ARGV_HISTORY ) != argvMap.end()) {
        readHistory( argvMap[ARGV_HISTORY], input.history );
    }
    if (argvMap.find( ARGV_WEEKDATA ) != argvMap.end()) {
        readWeekData( argvMap[ARGV_WEEKDATA], input.weekData );
    }
    if (argvMap.find( ARGV_SOLUTION ) != argvMap.end()) {
        input.solutionFileName = argvMap[ARGV_SOLUTION];
    }
    if (argvMap.find( ARGV_CUSTOM_INPUT ) != argvMap.end()) {
        psolver = readCustomInput( argvMap[ARGV_CUSTOM_INPUT] );
    }
    if (argvMap.find( ARGV_CUSTOM_OUTPUT ) != argvMap.end()) {
        input.customOutputFileName = argvMap[ARGV_CUSTOM_OUTPUT];
    }
    if (argvMap.find( ARGV_RANDOM_SEED ) != argvMap.end()) {
        istringstream iss( argvMap[ARGV_RANDOM_SEED] );
        iss >> input.randSeed;
    }

    // start computation
    ofstream csvFile( LOG_FILE, ios::app );
    NurseRostering::Solver::initResultSheet( csvFile );

    run( 0, csvFile );

    // do cleaning
    csvFile.close();
    delete psolver;

    system( "pause" );
    return 0;
}


void run( int inst, std::ofstream &logFile )
{

}

void readScenario( const std::string &scenarioFileName, NurseRostering::Scenario &scenario )
{
    char c;
    char buf[MAX_BUF_LEN];
    ifstream ifs( scenarioFileName );

    ifs.getline( buf, MAX_BUF_LEN );        // SCENARIO = nXXXwX
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // WEEKS =
    ifs >> scenario.weekNum;
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // SKILLS =
    int skillNum;
    ifs >> skillNum;
    scenario.skillNames.resize( skillNum );
    for (int i = 0; i < skillNum; i++) {
        ifs >> scenario.skillNames[i];
    }
    ifs.getline( buf, MAX_BUF_LEN, '=' );   // SHIFT_TYPES =
    int shiftTypeNum;
    ifs >> shiftTypeNum;
    scenario.shifts.resize( shiftTypeNum );
    for (int i = 0; i < shiftTypeNum; i++) {
        ifs >> scenario.shifts[i].name >> c;
    }

    ifs.close();
}

void readHistory( const std::string &historyFileName, NurseRostering::History &history )
{

}

void readWeekData( const std::string &weekDataFileName, NurseRostering::WeekData &weekdata )
{

}

NurseRostering::Solver* readCustomInput( const std::string &customInputFileName )
{

}
