#include "INRC2_Debug.h"


using namespace std;
using namespace INRC2;


int main()
{
    srand( static_cast<int>(time( NULL ) + clock()) );
    //analyzeInstance();

    unsigned instIndex = 11;
    char initHis = '1';
    char weekdata[10] = "7002";
    int randSeed = 26888;

    test_customIO( testOutputDir, instIndex, initHis, weekdata, 0, randSeed );
    test_customIO( testOutputDir, instIndex, initHis, weekdata, instTimeout[instIndex], randSeed );

    //for (instIndex = 0; instIndex < instance.size(); ++instIndex) {
    //    initHis = genInitHisIndex();
    //    genWeekdataSequence( instIndex, weekdata );
    //    randSeed = rand();
    //    test_customIO( testOutputDir, instIndex, initHis, weekdata, 0, randSeed );
    //    test_customIO( testOutputDir, instIndex, initHis, weekdata, instTimeout[instIndex], randSeed );
    //}

    //system( "pause" );
    return 0;
}