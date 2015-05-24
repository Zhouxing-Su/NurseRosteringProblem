/**
*   usage : 1.  invoke solver interface to solve problem.
*           2.  command line arguments : see help()
*
*   note :  1.
*/

#ifndef INRC2_H
#define INRC2_H


#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>

#include "DebugFlag.h"
#include "NurseRostering.h"
#include "Solution.h"
#include "Solver.h"


namespace INRC2
{
    const int MAX_BUF_SIZE = 1000;   // max size for char array buf
    const int MAX_BUF_LEN = MAX_BUF_SIZE - 1;   // max length for char array buf

    extern const std::string ARGV_SCENARIO;
    extern const std::string ARGV_HISTORY;
    extern const std::string ARGV_WEEKDATA;
    extern const std::string ARGV_SOLUTION;
    extern const std::string ARGV_CUSTOM_INPUT;
    extern const std::string ARGV_CUSTOM_OUTPUT;
    extern const std::string ARGV_RANDOM_SEED;
    extern const std::string ARGV_TIME;  // in seconds
    extern const std::string ARGV_ITER;

    extern const std::string weekdayNames[NurseRostering::Weekday::SIZE];
    extern const std::map<std::string, int> weekdayMap;


    int run( int argc, char *argv[] );

    bool readScenario( const std::string &scenarioFileName, NurseRostering &input );
    bool readHistory( const std::string &historyFileName, NurseRostering &input );
    bool readWeekData( const std::string &weekDataFileName, NurseRostering &input );
    bool readCustomInput( const std::string &customInputFileName, NurseRostering &input );
    bool writeSolution( const std::string &solutionFileName, const NurseRostering::Solver &solver );
    bool writeCustomOutput( const std::string &customOutputFileName, const NurseRostering::Solver &solver );
}


#endif