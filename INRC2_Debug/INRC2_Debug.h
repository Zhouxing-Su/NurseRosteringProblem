/**
*   usage : 1. just for test...
*
*   note :  1. use some bad tricks to reduce duplicated work.
*/

#ifndef INRC2_DEBUG_H
#define INRC2_DEBUG_H


#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>

#include "INRC2.h"


enum ArgcVal
{
    single = 2,
    full = 17,
    noRand = full - single,
    noCusIn = full - single,
    noCusOut = full - single,
    noCusInCusOut = full - 2 * single,
    noRandCusIn = full - 2 * single,
    noRandCusOut = full - 2 * single,
    noRandCusInCusOut = noCusInCusOut - single
};

extern char *fullArgv[ArgcVal::full];

extern const char *testOutputDir;
extern const std::string dir;
extern const std::vector<std::string> instance;

extern const std::string scePrefix;
extern const std::string weekPrefix;
extern const std::string initHisPrefix;
extern const std::string hisPrefix;
extern const std::string solPrefix;
extern const std::string fileSuffix;



void analyzeInstance();

void test( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec );
void test_customIO( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec );
void prepareArgv_FirstWeek( const char *outputDir, char *argv[], int instIndex, char initHis, char week, std::string timeoutInSec );
void prepareArgv( const char *outputDir, char *argv[], int instIndex, const char *weeks, char week, std::string timeoutInSec );


#endif