/**
*   usage : 1. simulator for calling solver on stages.
*
*   note :  1.
*/

#ifndef INRC2_DEBUG_H
#define INRC2_DEBUG_H


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>

#include "DebugFlag.h"
#include "INRC2.h"


enum ArgcVal
{
    single = 2,
    full = 19,
    noRand = full - single,
    noCusIn = full - single,
    noCusOut = full - single,
    noCusInCusOut = full - 2 * single,
    noRandCusIn = full - 2 * single,
    noRandCusOut = full - 2 * single,
    noRandCusInCusOut = noCusInCusOut - single
};

enum ArgvIndex
{
    program = 0, __id, id, __sce, sce, __his, his, __week, week, __sol, sol,
    __timout, timeout, __randSeed, randSeed, __cusIn, cusIn, __cusOut, cusOut
};

static const int MAX_ARGV_LEN = 256;
static const int INIT_HIS_NUM = 3;
static const int WEEKDATA_NUM = 10;
static const int MAX_WEEK_NUM = 8;
static const int WEEKDATA_SEQ_SIZE = (MAX_WEEK_NUM + 1);

extern char *fullArgv[ArgcVal::full];

extern const std::string outputDirPrefix;
extern const std::string instanceDir;
extern const std::vector<std::string> instance;

extern const std::vector<double> instTimeout;

extern const std::string scePrefix;
extern const std::string weekPrefix;
extern const std::string initHisPrefix;
extern const std::string hisPrefix;
extern const std::string solPrefix;
extern const std::string fileSuffix;
extern const std::string cusPrefix;

extern const char *FeasibleCheckerHost;


// this functions do not guarantee the sequence is feasible
char genInitHisIndex();
void genWeekdataSequence( int instIndex, char weekdata[WEEKDATA_SEQ_SIZE] );
// this function will make sure the sequence is feasible
void genInstanceSequence( int instIndex, char &initHis, char weekdata[WEEKDATA_SEQ_SIZE] );

void makeSureDirExist( const std::string &dir );

void testAllInstances( const std::string &id, int runCount );
void test( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec );
void test( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec, int randSeed );
void test_customIO( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec );
void test_customIO( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec, int randSeed );
void prepareArgv_FirstWeek( const std::string &id, const std::string &outputDir, char *argv[], char argvBuf[][MAX_ARGV_LEN], int instIndex, char initHis,
    char week, const std::string &timeoutInSec, const std::string &randSeed = "", const std::string &cusOut = "" );
void prepareArgv( const std::string &id, const std::string &outputDir, char *argv[], char argvBuf[][MAX_ARGV_LEN], int instIndex, const char *weeks, char week,
    const std::string &timeoutInSec, const std::string &randSeed = "", const std::string &cusIn = "", const std::string &cusOut = "" );

void httpget( std::ostream &os, const char *host, const char *file );

#endif