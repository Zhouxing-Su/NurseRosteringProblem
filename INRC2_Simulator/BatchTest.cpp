#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x601  // Win7
#endif

#include <boost/asio.hpp>

#include "BatchTest.h"


using namespace std;
using namespace INRC2;

using boost::asio::ip::tcp;


char *fullArgv[ArgcVal::full] = {
    "NurseRostering.exe",
    "--id", "",
    "--sce", "",
    "--his", "",
    "--week", "",
    "--sol", "",
    "--timeout", "",
    "--rand", "",
    "--cusIn", "",
    "--cusOut", ""
};

const std::string outputDirPrefix( "output" );
const std::string instanceDir( "../NurseRostering/instance/" );
const std::vector<std::string> instance = {
    "n005w4", "n012w8", "n021w4", // 0 1 2
    "n030w4", "n030w8", // 3 4
    "n040w4", "n040w8", // 5 6
    "n050w4", "n050w8", // 7 8
    "n060w4", "n060w8", // 9 10
    "n080w4", "n080w8", // 11 12
    "n100w4", "n100w8", // 13 14
    "n120w4", "n120w8"  // 15 16
};

const std::string timoutFileName( "timeout.txt" );
std::map<int, double> instTimeout;
const std::string instSeqFileName( "seq.txt" );
std::vector<char> instInitHis;
std::vector<std::string> instWeekdataSeq;

const std::string scePrefix( "/Sc-" );
const std::string weekPrefix( "/WD-" );
const std::string initHisPrefix( "/H0-" );
const std::string hisPrefix( "/history-week" );
const std::string solPrefix( "/sol-week" );
const std::string fileSuffix( ".txt" );
const std::string cusPrefix( "/custom-week" );

const char *FeasibleCheckerHost = "themis.playhost.be";


void testAllInstancesWithPreloadedInstSeq( const std::string &id, int runCount )
{
    unsigned instIndex;
    int randSeed;

    for (instIndex = 0; instIndex < instance.size(); ++instIndex) {
        for (int i = runCount; i > 0; --i) {
            randSeed = static_cast<int>(rand() + time( NULL ) + clock());
            test_customIO( id, outputDirPrefix + id, instIndex,
                instInitHis[instIndex], instWeekdataSeq[instIndex].c_str(), instTimeout[getNurseNum( instIndex )], randSeed );
        }
    }
}

void testAllInstances( const std::string &id, int runCount, int seedForInstSeq )
{
    unsigned instIndex;
    char initHis;
    char weekdata[WEEKDATA_SEQ_SIZE];
    int randSeed;

    for (instIndex = 0; instIndex < instance.size(); ++instIndex) {
        for (int i = runCount; i > 0; --i) {
            srand( seedForInstSeq );
            seedForInstSeq = rand();
            genInstanceSequence( instIndex, initHis, weekdata );
            randSeed = static_cast<int>(rand() + time( NULL ) + clock());
            test_customIO( id, outputDirPrefix + id, instIndex,
                initHis, weekdata, instTimeout[getNurseNum( instIndex )], randSeed );
        }
    }
}

void loadInstTimeOut()
{
    ifstream timoutFile( timoutFileName );

    int nurseNum;
    double runningTime;
    while (timoutFile >> nurseNum >> runningTime) {
        instTimeout[nurseNum] = runningTime;
    }

    timoutFile.close();
}

void loadInstSeq()
{
    ifstream instSeqFile( instSeqFileName );

    std::string instName;
    char initHis;
    std::string weekdataSeq;
    while (instSeqFile >> instName >> initHis >> weekdataSeq) {
        instInitHis.push_back( initHis );
        instWeekdataSeq.push_back( weekdataSeq );
    }

    instSeqFile.close();
}

int getNurseNum( int instIndex )
{
    istringstream iss( instance[instIndex].substr( 1, 3 ) );
    int nurseNum;
    iss >> nurseNum;
    return nurseNum;
}

int getWeekNum( int instIndex )
{
    return instance[instIndex][5] - '1';
}

char genInitHisIndex()
{
    return ((rand() % INIT_HIS_NUM) + '0');
}

void genWeekdataSequence( int instIndex, char weekdata[WEEKDATA_SEQ_SIZE] )
{
    memset( weekdata, 0, WEEKDATA_SEQ_SIZE * sizeof( char ) );
    int weekNum = getWeekNum( instIndex );
    for (; weekNum >= 0; --weekNum) {
        weekdata[weekNum] = (rand() % WEEKDATA_NUM) + '0';
    }
}

void genInstanceSequence( int instIndex, char &initHis, char weekdata[WEEKDATA_SEQ_SIZE] )
{
    bool feasible = false;
    do {
        initHis = genInitHisIndex();
        genWeekdataSequence( instIndex, weekdata );

        string file = "/" + instance[instIndex] + "/H0-" + instance[instIndex] + "-" + initHis;
        for (int weekNum = getWeekNum( instIndex ); weekNum >= 0; --weekNum) {
            file += ("/WD-" + instance[instIndex] + "-" + weekdata[weekNum]);
        }

        stringstream ss;
        httpget( ss, FeasibleCheckerHost, file.c_str() );

        char buf[256];
        ss.getline( buf, 255 );
        if (strstr( buf, "Feasible" )) {
            feasible = true;
        } else if (strstr( buf, "Infeasible" )) {
            feasible = false;
        } else {
            cerr << getTime() << " : error in response from feasible checker." << endl;
        }
    } while (!feasible);
}

void makeSureDirExist( const string &dir )
{
#ifdef WIN32
    system( ("mkdir \"" + dir + "\" 2> nul").c_str() );
#else
    system( ("mkdir -p \"" + dir + "\" 2> /dev/null").c_str() );
#endif
}

void test( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec )
{
    makeSureDirExist( outputDir );
    ostringstream t;
    t << timeoutInSec;

    char *argv[ArgcVal::full];
    char argvBuf[ArgcVal::full][MAX_ARGV_LEN];
    int argc = ArgcVal::noCusInCusOut;

    prepareArgv_FirstWeek( id, outputDir, argv, argvBuf, instIndex, initHis, weeks[0], t.str() );
    run( argc, argv );

    for (char w = '1'; w < instance[instIndex][5]; w++) {
        prepareArgv( id, outputDir, argv, argvBuf, instIndex, weeks, w, t.str() );
        run( argc, argv );
    }
}

void test( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec, int randSeed )
{
    makeSureDirExist( outputDir );
    ostringstream t, r;
    t << timeoutInSec;
    r << randSeed;

    char *argv[ArgcVal::full];
    char argvBuf[ArgcVal::full][MAX_ARGV_LEN];
    int argc = ArgcVal::noCusInCusOut;

    prepareArgv_FirstWeek( id, outputDir, argv, argvBuf, instIndex, initHis, weeks[0], t.str(), r.str() );
    run( argc, argv );

    for (char w = '1'; w < instance[instIndex][5]; w++) {
        prepareArgv( id, outputDir, argv, argvBuf, instIndex, weeks, w, t.str(), r.str() );
        run( argc, argv );
    }
}

void test_customIO( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec )
{
    makeSureDirExist( outputDir );
    ostringstream t;
    t << timeoutInSec;

    int argc;
    char *argv[ArgcVal::full];
    char argvBuf[ArgcVal::full][MAX_ARGV_LEN];

    argc = ArgcVal::noRandCusIn;
    prepareArgv_FirstWeek( id, outputDir, argv, argvBuf, instIndex, initHis, weeks[0],
        t.str(), "", (outputDir + cusPrefix + '0') );
    run( argc, argv );

    char w = '1';
    for (; w < (instance[instIndex][5] - 1); w++) {
        argc = ArgcVal::noRand;
        prepareArgv( id, outputDir, argv, argvBuf, instIndex, weeks, w, t.str(), "",
            outputDir + cusPrefix + static_cast<char>(w - 1), outputDir + cusPrefix + w );
        run( argc, argv );
    }

    argc = ArgcVal::noRandCusOut;
    prepareArgv( id, outputDir, argv, argvBuf, instIndex, weeks, w, t.str(), "",
        outputDir + cusPrefix + static_cast<char>(w - 1), "" );
    run( argc, argv );
}

void test_customIO( const std::string &id, const std::string &outputDir, int instIndex, char initHis, const char *weeks, double timeoutInSec, int randSeed )
{
    makeSureDirExist( outputDir );
    ostringstream t, r;
    t << timeoutInSec;
    r << randSeed;

    int argc;
    char *argv[ArgcVal::full];
    char argvBuf[ArgcVal::full][MAX_ARGV_LEN];

    argc = ArgcVal::noCusIn;
    prepareArgv_FirstWeek( id, outputDir, argv, argvBuf, instIndex, initHis, weeks[0],
        t.str(), r.str(), (outputDir + cusPrefix + '0') );
    run( argc, argv );

    char w = '1';
    for (; w < (instance[instIndex][5] - 1); w++) {
        argc = ArgcVal::full;
        prepareArgv( id, outputDir, argv, argvBuf, instIndex, weeks, w, t.str(), r.str(),
            outputDir + cusPrefix + static_cast<char>(w - 1), outputDir + cusPrefix + w );
        run( argc, argv );
    }

    argc = ArgcVal::noCusOut;
    prepareArgv( id, outputDir, argv, argvBuf, instIndex, weeks, w, t.str(), r.str(),
        outputDir + cusPrefix + static_cast<char>(w - 1), "" );
    run( argc, argv );
}

void prepareArgv_FirstWeek( const std::string &id, const std::string &outputDir, char *argv[], char argvBuf[][MAX_ARGV_LEN], int i, char h, char w, const std::string &t, const std::string &r, const std::string &co )
{
    string sce = instanceDir + instance[i] + scePrefix + instance[i] + fileSuffix;
    string his = instanceDir + instance[i] + initHisPrefix + instance[i] + '-' + h + fileSuffix;
    string week = instanceDir + instance[i] + weekPrefix + instance[i] + '-' + w + fileSuffix;
    string sol = outputDir + solPrefix + "0" + fileSuffix;
    i = 0;
    argv[i] = fullArgv[ArgvIndex::program];
    argv[++i] = fullArgv[ArgvIndex::__id];
    strcpy( argvBuf[++i], id.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__sce];
    strcpy( argvBuf[++i], sce.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__his];
    strcpy( argvBuf[++i], his.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__week];
    strcpy( argvBuf[++i], week.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__sol];
    strcpy( argvBuf[++i], sol.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__timout];
    strcpy( argvBuf[++i], t.c_str() );
    argv[i] = argvBuf[i];
    if (!r.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__randSeed];
        strcpy( argvBuf[++i], r.c_str() );
        argv[i] = argvBuf[i];
    }
    if (!co.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__cusOut];
        strcpy( argvBuf[++i], co.c_str() );
        argv[i] = argvBuf[i];
    }
}

void prepareArgv( const std::string &id, const std::string &outputDir, char *argv[], char argvBuf[][MAX_ARGV_LEN], int i, const char *weeks, char w, const std::string &t, const std::string &r, const std::string &ci, const std::string &co )
{
    string sce = instanceDir + instance[i] + scePrefix + instance[i] + fileSuffix;
    string week = instanceDir + instance[i] + weekPrefix + instance[i] + '-' + weeks[w - '0'] + fileSuffix;
    string sol = outputDir + solPrefix + w + fileSuffix;
    string his = outputDir + hisPrefix + (--w) + fileSuffix;
    i = 0;
    argv[i] = fullArgv[ArgvIndex::program];
    argv[++i] = fullArgv[ArgvIndex::__id];
    strcpy( argvBuf[++i], id.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__sce];
    strcpy( argvBuf[++i], sce.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__his];
    strcpy( argvBuf[++i], his.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__week];
    strcpy( argvBuf[++i], week.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__sol];
    strcpy( argvBuf[++i], sol.c_str() );
    argv[i] = argvBuf[i];
    argv[++i] = fullArgv[ArgvIndex::__timout];
    strcpy( argvBuf[++i], t.c_str() );
    argv[i] = argvBuf[i];
    if (!r.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__randSeed];
        strcpy( argvBuf[++i], r.c_str() );
        argv[i] = argvBuf[i];
    }
    if (!ci.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__cusIn];
        strcpy( argvBuf[++i], ci.c_str() );
        argv[i] = argvBuf[i];
    }
    if (!co.empty()) {
        argv[++i] = fullArgv[ArgvIndex::__cusOut];
        strcpy( argvBuf[++i], co.c_str() );
        argv[i] = argvBuf[i];
    }
}

void httpget( ostream &os, const char *host, const char *file )
{
    try {
        boost::asio::io_service io_service;

        // Get a list of endpoints corresponding to the server name.
        tcp::resolver resolver( io_service );
        tcp::resolver::query query( host, "http" );
        tcp::resolver::iterator endpoint_iterator = resolver.resolve( query );
        tcp::resolver::iterator end;

        // Try each endpoint until we successfully establish a connection.
        tcp::socket socket( io_service );
        boost::system::error_code error = boost::asio::error::host_not_found;
        while (error && endpoint_iterator != end) {
            socket.close();
            socket.connect( *endpoint_iterator++, error );
        }
        if (error) {
            throw boost::system::system_error( error );
        }

        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        boost::asio::streambuf request;
        std::ostream request_stream( &request );
        request_stream << "GET " << file << " HTTP/1.0\r\n";
        request_stream << "Host: " << host << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n\r\n";

        // Send the request.
        boost::asio::write( socket, request );

        // Read the response status line.
        boost::asio::streambuf response;
        boost::asio::read_until( socket, response, "\r\n" );

        // Check that response is OK.
        std::istream response_stream( &response );
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        std::string status_message;
        std::getline( response_stream, status_message );
        if (!response_stream || http_version.substr( 0, 5 ) != "HTTP/") {
            std::cerr << "Invalid response\n";
            return;
        }
        if (status_code != 200) {
            std::cerr << "Response returned with status code " << status_code << "\n";
            return;
        }

        // Read the response headers, which are terminated by a blank line.
        boost::asio::read_until( socket, response, "\r\n\r\n" );

        // Process the response headers.
        std::string header;
        while (std::getline( response_stream, header ) && header != "\r") {}

        // Write whatever content we already have to output.
        if (response.size() > 0) {
            os << &response;
        }

        // Read until EOF, writing data to output as we go.
        while (boost::asio::read( socket, response,
            boost::asio::transfer_at_least( 1 ), error )) {
            os << &response;
        }
        if (error != boost::asio::error::eof) {
            throw boost::system::system_error( error );
        }

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}
