#include "INRC2_Debug.h"


using namespace std;


int main()
{
    test_n005w4_1233();

    system( "pause" );
    return 0;
}


#define INSTANCE "n005w4"
#define OUTPUT "Sln_n005w4_h0_w1233_run0"
void test_n005w4_1233()
{
    char *argv0[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", DIR "/" INSTANCE "/" "H0-n005w4-0.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "1" ),
        "--sol", OUTPUT "/" "Sol0.txt",
        "--cusOut", OUTPUT "/" "custom1"
    };  // 11

    char *argv1[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", OUTPUT "/" "His1.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "2" ),
        "--sol", OUTPUT "/" "Sol1.txt",
        "-cusIn", OUTPUT "/" "custom1",
        "--cusOut", OUTPUT "/" "custom2"
    };  // 13

    char *argv2[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", OUTPUT "/" "His2.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "3" ),
        "--sol", OUTPUT "/" "Sol2.txt",
        "-cusIn", OUTPUT "/" "custom2",
        "--cusOut", OUTPUT "/" "custom3"
    };  // 13

    char *argv3[80] = {
        "INRC2.exe",
        "--sce", DIR "/" INSTANCE "/" SCENARIO( INSTANCE ),
        "--his", OUTPUT "/" "His3.txt",
        "--week", DIR "/" INSTANCE "/" WEEKDATA( INSTANCE, "3" ),
        "--sol", OUTPUT "/" "Sol3.txt",
        "-cusIn", OUTPUT "/" "custom3"
    };  // 11

    run( 11, argv0, "n005w4_h0_w1233_0" );
    run( 13, argv1, "n005w4_h0_w1233_1" );
    run( 13, argv2, "n005w4_h0_w1233_2" );
    run( 11, argv3, "n005w4_h0_w1233_3" );
}
#undef OUTPUT
#undef INSTANCE