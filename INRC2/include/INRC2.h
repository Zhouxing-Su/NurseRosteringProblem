/**
*   usage : 1. invoke solver interface to solve problem.
*
*   note :  1. remove code with [!] which is code for debugging
*/

#ifndef INRC2_H
#define INRC2_H


#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>

#include "NurseRostering.h"


namespace INRC2
{
    const int MAX_BUF_SIZE = 1000;   // max size for char array buf
    const int MAX_BUF_LEN = MAX_BUF_SIZE - 1;   // max length for char array buf

    const std::string LOG_FILE( "log.csv" );

    const std::string ARGV_SCENARIO( "sce" );
    const std::string ARGV_HISTORY( "his" );
    const std::string ARGV_WEEKDATA( "week" );
    const std::string ARGV_SOLUTION( "sol" );
    const std::string ARGV_CUSTOM_INPUT( "cusIn" );
    const std::string ARGV_CUSTOM_OUTPUT( "cusOut" );
    const std::string ARGV_RANDOM_SEED( "rand" );
    const std::string ARGV_TIME( "timeout" );  // in seconds

    const std::string weekdayNames[NurseRostering::WEEKDAY_NUM] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };
    const std::map<std::string, int> weekdayMap = {
        { weekdayNames[0], 0 }, { weekdayNames[1], 1 }, { weekdayNames[2], 2 }, { weekdayNames[3], 3 },
        { weekdayNames[4], 4 }, { weekdayNames[5], 5 }, { weekdayNames[6], 6 },
    };

    void run( int argc, char *argv[] );

    void readScenario( const std::string &scenarioFileName, NurseRostering &input );
    void readHistory( const std::string &historyFileName, NurseRostering &input );
    void readWeekData( const std::string &weekDataFileName, NurseRostering &input );
    void readCustomInput( const std::string &customInputFileName, NurseRostering &input );
    void writeSolution( const std::string &solutionFileName, const NurseRostering::Solver &solver );
    void writeCustomOutput( const std::string &customOutputFileName );
}


#endif