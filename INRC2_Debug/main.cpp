#include "INRC2_Debug.h"


using namespace std;
using namespace INRC2;


int main()
{
    test_n005w4_1233();

    system( "pause" );
    return 0;
}


#define INSTANCE "n005w4"
#define OUTPUT "output"
void test_n005w4_1233()
{
    char *argv0[] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", DIR "/" INSTANCE "/" HISTORY( INSTANCE, "0" ),
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "2" ),
        "--sol", OUTPUT "/" "sol-week0.txt",
        //"--cusOut", OUTPUT "/" "custom1",
        "--rand", "0",
        "--timeout", "10"
    };

    char *argv1[] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", OUTPUT "/" "history-week0.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "0" ),
        "--sol", OUTPUT "/" "sol-week1.txt",
        //"-cusIn", OUTPUT "/" "custom1",
        //"--cusOut", OUTPUT "/" "custom2",
        "--rand", "0",
        "--timeout", "10"
    };

    char *argv2[] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", OUTPUT "/" "history-week1.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "2" ),
        "--sol", OUTPUT "/" "sol-week2.txt",
        //"-cusIn", OUTPUT "/" "custom2",
        //"--cusOut", OUTPUT "/" "custom3",
        "--rand", "0",
        "--timeout", "10"
    };

    char *argv3[] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", OUTPUT "/" "history-week2.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "1" ),
        "--sol", OUTPUT "/" "sol-week3.txt",
        //"-cusIn", OUTPUT "/" "custom3",
        "--rand", "0",
        "--timeout", "10"
    };

    run( sizeof( argv0 ) / sizeof( void* ), argv0 );
    run( sizeof( argv1 ) / sizeof( void* ), argv1 );
    run( sizeof( argv2 ) / sizeof( void* ), argv2 );
    run( sizeof( argv3 ) / sizeof( void* ), argv3 );
}
#undef OUTPUT
#undef INSTANCE