#include "INRC2_Debug.h"


using namespace std;
using namespace INRC2;


int main()
{
    srand( static_cast<int>(time( NULL ) + clock()) );
    //analyzeInstance();

    unsigned instIndex = 0;
    char initHis = '0';
    char weekdata[10] = "2608";
    int randSeed = 19934;

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