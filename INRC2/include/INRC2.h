/**
*   usage : 1. invoke solver interface to solve problem.
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
#include <cstdlib>
#include <ctime>

#include "NurseRostering.h"


const int MAX_BUF_SIZE = 1000;   // max size for char array buf
const int MAX_BUF_LEN = MAX_BUF_SIZE - 1;   // max length for char array buf

const std::string LOG_FILE = "log.csv";

const std::string ARGV_SCENARIO = "sce";
const std::string ARGV_HISTORY = "his";
const std::string ARGV_WEEKDATA = "week";
const std::string ARGV_SOLUTION = "sol";
const std::string ARGV_CUSTOM_INPUT = "cusIn";
const std::string ARGV_CUSTOM_OUTPUT = "cusOut";
const std::string ARGV_RANDOM_SEED = "rand";


void run( int argc, char *argv[], const std::string &instance = std::string() );

void readScenario( const std::string &scenarioFileName, NurseRostering &input );
void readHistory( const std::string &historyFileName, NurseRostering &input );
void readWeekData( const std::string &weekDataFileName, NurseRostering &input );
NurseRostering::Solver* readCustomInput( const std::string &customInputFileName );


#endif