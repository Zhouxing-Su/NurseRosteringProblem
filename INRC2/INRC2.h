#ifndef INRC2_H
#define INRC2_H


#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <cstdlib>

#include "NurseRostering.h"


const int MAX_BUF_LEN = 1000;   // max length for char array buf

const std::string LOG_FILE = "log.csv";

const std::string ARGV_SCENARIO = "sce";
const std::string ARGV_HISTORY = "his";
const std::string ARGV_WEEKDATA = "week";
const std::string ARGV_SOLUTION = "sol";
const std::string ARGV_CUSTOM_INPUT = "cusIn";
const std::string ARGV_CUSTOM_OUTPUT = "cusOut";
const std::string ARGV_RANDOM_SEED = "rand";


void run( int inst, std::ofstream &logFile );

void readScenario( const std::string &scenarioFileName, NurseRostering::Scenario &scenario );
void readHistory( const std::string &historyFileName, NurseRostering::History &history );
void readWeekData( const std::string &weekDataFileName, NurseRostering::WeekData &weekdata );
NurseRostering::Solver* readCustomInput( const std::string &customInputFileName );


#endif