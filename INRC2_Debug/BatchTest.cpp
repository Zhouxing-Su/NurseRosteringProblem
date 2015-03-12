#include "INRC2_Debug.h"


using namespace std;
using namespace INRC2;


void makeSureDirExist( const string &dir )
{
    static const std::string mkdir_win32cmd( "mkdir " );
    static const std::string mkdir_unixshell( "mkdir -p " );
    system( (mkdir_win32cmd + dir).c_str() );
    system( (mkdir_unixshell + dir).c_str() );
}

void test( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec, int randSeed )
{
    makeSureDirExist( outputDir );
    ostringstream t, r;
    t << timeoutInSec;
    r << randSeed;

    char *argv[ArgcVal::full];
    int argc = ArgcVal::noCusInCusOut;

    prepareArgv_FirstWeek( outputDir, argv, instIndex, initHis, weeks[0], t.str(), r.str() );
    run( argc, argv );

    for (char w = '1'; w < instance[instIndex][5]; w++) {
        prepareArgv( outputDir, argv, instIndex, weeks, w, t.str(), r.str() );
        run( argc, argv );
    }
}

void test_customIO( const char *outputDir, int instIndex, char initHis, const char *weeks, int timeoutInSec, string cusIn, string cusOut )
{
    makeSureDirExist( outputDir );
    ostringstream t;
    t << timeoutInSec;

    int argc;
    char *argv[ArgcVal::full];

    argc = ArgcVal::noRandCusIn;
    prepareArgv_FirstWeek( outputDir, argv, instIndex, initHis, weeks[0], t.str(), "", cusIn, cusOut );
    run( argc, argv );

    char w = '1';
    for (; w < (instance[instIndex][5] - 1); w++) {
        argc = ArgcVal::noRand;
        prepareArgv( outputDir, argv, instIndex, weeks, w, t.str(), "", cusIn, cusOut );
        run( argc, argv );
    }

    argc = ArgcVal::noRandCusOut;
    prepareArgv( outputDir, argv, instIndex, weeks, w, t.str(), "", cusIn, cusOut );
    run( argc, argv );
}

void prepareArgv_FirstWeek( const char *outputDir, char *argv[], int i, char h, char w, string t, string r, string ci, string co )
{
    string sce = dir + instance[i] + scePrefix + instance[i] + fileSuffix;
    string his = dir + instance[i] + initHisPrefix + instance[i] + '-' + h + fileSuffix;
    string week = dir + instance[i] + weekPrefix + instance[i] + '-' + w + fileSuffix;
    string sol = outputDir + solPrefix + "0" + fileSuffix;
    i = 0;
    argv[i] = fullArgv[ArgvIndex::program];
    argv[++i] = fullArgv[ArgvIndex::__sce];
    argv[++i] = new char[sce.size() + 1];
    strcpy( argv[i], sce.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__his];
    argv[i] = new char[his.size() + 1];
    strcpy( argv[i], his.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__week];
    argv[++i] = new char[week.size() + 1];
    strcpy( argv[i], week.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__sol];
    argv[++i] = new char[sol.size() + 1];
    strcpy( argv[i], sol.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__timout];
    argv[++i] = new char[t.size() + 1];
    strcpy( argv[i], t.c_str() );
    if (r.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__rand];
        argv[++i] = new char[r.size() + 1];
        strcpy( argv[i], r.c_str() );
    }
    if (r.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__rand];
        argv[++i] = new char[r.size() + 1];
        strcpy( argv[i], r.c_str() );
    }
    if (ci.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__cusIn];
        argv[++i] = new char[ci.size() + 1];
        strcpy( argv[i], ci.c_str() );
    }
    if (co.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__cusOut];
        argv[++i] = new char[co.size() + 1];
        strcpy( argv[i], co.c_str() );
    }
}

void prepareArgv( const char *outputDir, char *argv[], int i, const char *weeks, char w, string t, string r, string ci, string co )
{
    string sce = dir + instance[i] + scePrefix + instance[i] + fileSuffix;
    string his = outputDir + hisPrefix + (--w) + fileSuffix;
    string week = dir + instance[i] + weekPrefix + instance[i] + '-' + weeks[w - '0'] + fileSuffix;
    string sol = outputDir + solPrefix + (++w) + fileSuffix;
    int i = 0;
    argv[i] = fullArgv[ArgvIndex::program];
    argv[++i] = fullArgv[ArgvIndex::__sce];
    argv[++i] = new char[sce.size() + 1];
    strcpy( argv[i], sce.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__his];
    argv[++i] = new char[his.size() + 1];
    strcpy( argv[i], his.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__week];
    argv[++i] = new char[week.size() + 1];
    strcpy( argv[i], week.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__sol];
    argv[++i] = new char[sol.size() + 1];
    strcpy( argv[i], sol.c_str() );
    argv[++i] = fullArgv[ArgvIndex::__timout];
    argv[++i] = new char[t.size() + 1];
    strcpy( argv[i], t.c_str() );
    if (r.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__rand];
        argv[++i] = new char[r.size() + 1];
        strcpy( argv[i], r.c_str() );
    }
    if (ci.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__cusIn];
        argv[++i] = new char[ci.size() + 1];
        strcpy( argv[i], ci.c_str() );
    }
    if (co.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__cusOut];
        argv[++i] = new char[co.size() + 1];
        strcpy( argv[i], co.c_str() );
    }
}