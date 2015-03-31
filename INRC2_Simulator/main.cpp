#include "BatchTest.h"


using namespace std;
using namespace INRC2;


int main()
{
    srand( static_cast<int>(time( NULL ) + clock()) );

    ostringstream id( "0" );

    //int instIndex = 2;
    //char initHis = '2';
    //char weekdata[WEEKDATA_SEQ_SIZE] = "7655";
    //int randSeed = 22489;
    //double runningTime = 30;
    //test_customIO( id.str(), outputDirPrefix + id.str(), instIndex, initHis, weekdata, runningTime, randSeed );

    vector<thread> vt;
    int threadNum = thread::hardware_concurrency() * 3 / 4;
    threadNum += (threadNum == 0);
    for (int i = 0; i < threadNum; ++i) {
        id.str( string() );
        id << i;
        vt.push_back( thread( testAllInstances, id.str(), 4 ) );
        this_thread::sleep_for( chrono::seconds( 3 ) );
    }

    for (int i = 0; i < threadNum; ++i) {
        vt[i].join();
    }

    //system( "pause" );
    return 0;
}