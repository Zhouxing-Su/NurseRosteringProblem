#include "BatchTest.h"


using namespace std;
using namespace INRC2;


// single instance and single thread for single run
// for debugging certain sequence with problem
void debugRun()
{
    ostringstream id( "0" );

    srand( static_cast<int>(time( NULL ) + clock()) );
    int instIndex = InstIndex::n005w4;
    char initHis = '0';
    char weekdata[WEEKDATA_SEQ_SIZE] = "5533";
    int randSeed = 1429107988;
    double runningTime = instTimeout[getNurseNum( instIndex )];
    test_customIO( id.str(), outputDirPrefix + id.str(), instIndex, initHis, weekdata, runningTime, randSeed );
}

// all instances with random weekdata sequence on multi-threads
// repeat many times to fully judge the performance
void realRun()
{
    ostringstream id;

    int runCount = 8;
    int seedForInstSeq = 222;
    int threadBeginInterval = 3;
    vector<thread> vt;
    int threadNum = thread::hardware_concurrency() / 2;
    threadNum += (threadNum == 0);
    for (int i = 0; i < threadNum; ++i) {
        id.str( string() );
        id << i;
        vt.push_back( thread( testAllInstances, id.str(), runCount, seedForInstSeq ) );
        this_thread::sleep_for( chrono::seconds( threadBeginInterval ) );
    }
    for (int i = 0; i < threadNum; ++i) {
        vt[i].join();
    }
}

// all instances with fixed weekdata sequence on multi-threads
// repeat less times to compare performance on certain sequence
void benchmarkRun()
{
    ostringstream id;

    int runCount = 4;
    int threadBeginInterval = 3;
    int threadNum = thread::hardware_concurrency() / 2;
    threadNum += (threadNum == 0);
    vector<thread> vt;
    for (int i = 0; i < threadNum; ++i) {
        id.str( string() );
        id << i;
        vt.push_back( thread( testAllInstancesWithPreloadedInstSeq, id.str(), runCount ) );
        this_thread::sleep_for( chrono::seconds( threadBeginInterval ) );
    }
    for (int i = 0; i < threadNum; ++i) {
        vt[i].join();
    }
}

// heterogeneous instances with fixed weekdata sequence on multi-threads
// repeat even less times to adjust arguments
void sprintRun()
{
    ostringstream id;

    int runCount = 2;
    int threadBeginInterval = 3;
    int threadNum = thread::hardware_concurrency() / 2;
    threadNum += (threadNum == 0);
    vector<thread> vt;
    for (int i = 0; i < threadNum; ++i) {
        id.str( string() );
        id << i;
        vt.push_back( thread( testHeterogeneousInstancesWithPreloadedInstSeq, id.str(), runCount ) );
        this_thread::sleep_for( chrono::seconds( threadBeginInterval ) );
    }
    for (int i = 0; i < threadNum; ++i) {
        vt[i].join();
    }
}

int main()
{
    loadConfig();
    loadInstTimeOut();
    loadInstSeq();
    FileLock::unlock( LOG_FILE_NAME );

    //debugRun();
    //realRun();
    //benchmarkRun();
    sprintRun();

    //system( "pause" );
    return 0;
}