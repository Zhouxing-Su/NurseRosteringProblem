#include "BatchTest.h"


using namespace std;
using namespace INRC2;


int main()
{
    loadInstTimeOut();
    loadInstSeq();
    FileLock::unlock( LOG_FILE_NAME );

    ostringstream id( "0" );

    // single instance and single thread for single run
    //srand( static_cast<int>(time( NULL ) + clock()) );
    //int instIndex = 3;
    //char initHis = '2';
    //char weekdata[WEEKDATA_SEQ_SIZE] = "6715";
    //int randSeed = rand();
    //double runningTime = instTimeout[getNurseNum( instIndex )];
    //test_customIO( id.str(), outputDirPrefix + id.str(), instIndex, initHis, weekdata, runningTime, randSeed );

    // all instances with random weekdata sequence on multi-threads
    //int runCount = 4;
    //int seedForInstSeq = 222;
    //int threadBeginInterval = 3;
    //vector<thread> vt;
    //int threadNum = thread::hardware_concurrency() * 3 / 4;
    //threadNum += (threadNum == 0);
    //for (int i = 0; i < threadNum; ++i) {
    //    id.str( string() );
    //    id << i;
    //    vt.push_back( thread( testAllInstances, id.str(), runCount, seedForInstSeq ) );
    //    this_thread::sleep_for( chrono::seconds( threadBeginInterval ) );
    //}
    //for (int i = 0; i < threadNum; ++i) {
    //    vt[i].join();
    //}

    int runCount = 4;
    int threadBeginInterval = 3;

    vector<thread> vt;
    int threadNum = thread::hardware_concurrency() / 2;
    threadNum += (threadNum == 0);
    for (int i = 0; i < threadNum; ++i) {
        id.str( string() );
        id << i;
        vt.push_back( thread( testAllInstancesWithPreloadedInstSeq, id.str(), runCount ) );
        this_thread::sleep_for( chrono::seconds( threadBeginInterval ) );
    }

    for (int i = 0; i < threadNum; ++i) {
        vt[i].join();
    }

    //system( "pause" );
    return 0;
}