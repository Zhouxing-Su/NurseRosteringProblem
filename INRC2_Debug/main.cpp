#include "INRC2_Debug.h"


using namespace std;
using namespace INRC2;


int main()
{
    srand( static_cast<int>(time( NULL ) + clock()) );
    //analyzeInstance();

    unsigned instIndex = 2;
    char initHis = '2';
    char weekdata[10] = "7655";
    int randSeed = 22489;

    test_customIO( testOutputDir, instIndex, initHis, weekdata, instTimeout[instIndex], randSeed );

    //for (instIndex = 0; instIndex < instance.size(); ++instIndex) {
    //    initHis = genInitHisIndex();
    //    genWeekdataSequence( instIndex, weekdata );
    //    randSeed = rand();
    //    test_customIO( testOutputDir, instIndex, initHis, weekdata, instTimeout[instIndex], randSeed );
    //}

    //system( "pause" );
    return 0;
}