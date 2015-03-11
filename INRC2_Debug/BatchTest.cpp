#include "INRC2_Debug.h"


using namespace std;
using namespace INRC2;


void test( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec )
{
    ostringstream t;
    t << timeoutInSec;

    char *argv[ArgcVal::full];
    int argc = ArgcVal::noRandCusInCusOut;;

    prepareArgv_FirstWeek( outputDir, argv, instIndex, initHis, weeks[0], t.str() );
    run( argc, argv );

    for (char w = '1'; w < instance[instIndex][5]; w++) {
        prepareArgv( outputDir, argv, instIndex, weeks, w, t.str() );
        run( argc, argv );
    }
}

void test_customIO( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec )
{
    ostringstream t;
    t << timeoutInSec;

    int argc;
    char *argv[ArgcVal::full];

    argc = ArgcVal::noRandCusIn;
    prepareArgv_FirstWeek( outputDir, argv, instIndex, initHis, weeks[0], t.str() );
    run( argc, argv );

    char w = '1';
    for (; w < (instance[instIndex][5] - 1); w++) {
        argc = ArgcVal::noRand;
        prepareArgv( outputDir, argv, instIndex, weeks, w, t.str() );
        run( argc, argv );
    }

    argc = ArgcVal::noRandCusOut;
    prepareArgv( outputDir, argv, instIndex, weeks, w, t.str() );
    run( argc, argv );
}

void prepareArgv_FirstWeek( const char *outputDir, char *argv[], int i, char h, char w, std::string t )
{
    string sce = dir + instance[i] + scePrefix + instance[i] + fileSuffix;
    string his = dir + instance[i] + initHisPrefix + instance[i] + '-' + h + fileSuffix;
    string week = dir + instance[i] + weekPrefix + instance[i] + '-' + w + fileSuffix;
    string sol = outputDir + solPrefix + "0" + fileSuffix;
    argv[0] = fullArgv[0];
    argv[1] = fullArgv[1];
    argv[2] = new char[sce.size() + 1];
    strcpy( argv[2], sce.c_str() );
    argv[3] = fullArgv[3];
    argv[4] = new char[his.size() + 1];
    strcpy( argv[4], his.c_str() );
    argv[5] = fullArgv[5];
    argv[6] = new char[week.size() + 1];
    strcpy( argv[6], week.c_str() );
    argv[7] = fullArgv[7];
    argv[8] = new char[sol.size() + 1];
    strcpy( argv[8], sol.c_str() );
    argv[9] = fullArgv[9];
    argv[10] = new char[t.size() + 1];
    strcpy( argv[10], t.c_str() );
}

void prepareArgv( const char *outputDir, char *argv[], int i, const char *weeks, char w, std::string t )
{
    string sce = dir + instance[i] + scePrefix + instance[i] + fileSuffix;
    string his = outputDir + hisPrefix + (--w) + fileSuffix;
    string week = dir + instance[i] + weekPrefix + instance[i] + '-' + weeks[w - '0'] + fileSuffix;
    string sol = outputDir + solPrefix + (++w) + fileSuffix;
    argv[0] = fullArgv[0];
    argv[1] = fullArgv[1];
    argv[2] = new char[sce.size() + 1];
    strcpy( argv[2], sce.c_str() );
    argv[3] = fullArgv[3];
    argv[4] = new char[his.size() + 1];
    strcpy( argv[4], his.c_str() );
    argv[5] = fullArgv[5];
    argv[6] = new char[week.size() + 1];
    strcpy( argv[6], week.c_str() );
    argv[7] = fullArgv[7];
    argv[8] = new char[sol.size() + 1];
    strcpy( argv[8], sol.c_str() );
    argv[9] = fullArgv[9];
    argv[10] = new char[t.size() + 1];
    strcpy( argv[10], t.c_str() );
}