/**
*   usage : 1. just for test...
*
*   note :  1. use some bad tricks to reduce duplicated work.
*/

#ifndef INRC2_DEBUG_H
#define INRC2_DEBUG_H


#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>

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

char *fullArgv[] = {
    "INRC2.exe",
    "--sce", "",
    "--his", "",
    "--week", "",
    "--sol", "",
    "--timeout", "",
    "--rand", "",
    "-cusIn", "",
    "--cusOut", ""
};  // 17

const char *testOutputDir = "output/";
const std::string dir( "../INRC2/instance/" );
const std::vector<std::string> instance = {
    "n005w4", "n012w8", "n021w4", "n030w4", "n030w8",
    "n040w4", "n040w8", "n050w4", "n050w8", "n060w4", "n060w8",
    "n080w4", "n080w8", "n100w4", "n100w8", "n120w4", "n120w8"
};

const std::string scePrefix( "/Sc-" );
const std::string weekPrefix( "/WD-" );
const std::string initHisPrefix( "/H0-" );
const std::string hisPrefix( "history-week" );
const std::string solPrefix( "sol-week" );
const std::string fileSuffix( ".txt" );

void prepareArgv_FirstWeek( const char *outputDir, char *argv[], int instIndex, char initHis, char week, std::string timeoutInSec );
void prepareArgv( const char *outputDir, char *argv[], int instIndex, const char *weeks, char week, std::string timeoutInSec );

void test( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec );
void test_customIO( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec );
void test_n005w4_1233();


#endif