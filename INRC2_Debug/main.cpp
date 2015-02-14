#include "INRC2_Debug.h"


using namespace std;


int main()
{
    test_n005w4_1233();

    system( "pause" );
    return 0;
}


#define INSTANCE "n005w4"
void test_n005w4_1233()
{
    char *argv0[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", DIR "/" INSTANCE "/" "H0-n005w4-0.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "1" ),
        "--sol", "Sol0.txt",
        "--cusOut", "custom1"
    };  // 11

    char *argv1[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", "His1.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "2" ),
        "--sol", "Sol1.txt",
        "-cusIn", "custom1",
        "--cusOut", "custom2"
    };  // 13

    char *argv2[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", "His2.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "3" ),
        "--sol", "Sol2.txt",
        "-cusIn", "custom2",
        "--cusOut", "custom3"
    };  // 13

    char *argv3[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", "His3.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "3" ),
        "--sol", "Sol3.txt",
        "-cusIn", "custom3"
    };  // 11

    run( 11, argv0 );
    run( 13, argv1 );
    run( 13, argv2 );
    run( 11, argv3 );
}
#undef INSTANCE