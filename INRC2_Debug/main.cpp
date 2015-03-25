#include "INRC2_Debug.h"


using namespace std;
using namespace INRC2;


char *fullArgv[ArgcVal::full] = {
    "INRC2.exe",
    "--sce", "",
    "--his", "",
    "--week", "",
    "--sol", "",
    "--timeout", "",
    "--rand", "",
    "--cusIn", "",
    "--cusOut", ""
};

const char *testOutputDir = "output/";
const std::string dir( "../INRC2/instance/" );
const std::vector<std::string> instance = {
    "n005w4", "n012w8", "n021w4", "n030w4", "n030w8",           // 5
    "n040w4", "n040w8", "n050w4", "n050w8", "n060w4", "n060w8", // 11
    "n080w4", "n080w8", "n100w4", "n100w8", "n120w4", "n120w8"  // 17
};

const std::vector<double> instTimeout = {
    60, 60, 60, 70.13, 70.13,
    122.73, 122.73, 175.34, 175.34, 227.94, 227.94,
    333.14, 333.14, 438.34, 438.34, 543.54, 543.54
};

const std::string scePrefix( "/Sc-" );
const std::string weekPrefix( "/WD-" );
const std::string initHisPrefix( "/H0-" );
const std::string hisPrefix( "history-week" );
const std::string solPrefix( "sol-week" );
const std::string fileSuffix( ".txt" );
const std::string cusPrefix( "custom-week" );


int main()
{
    //analyzeInstance();

    //int instIndex = 0;
    //test( testOutputDir, instIndex, '0', "1233", 0, 10 );
    //test( testOutputDir, instIndex, '0', "1233", instTimeout[instIndex], 10 );

    int instIndex = 0;
    test_customIO( testOutputDir, instIndex, '1', "0248", 0, 10 );
    test_customIO( testOutputDir, instIndex, '1', "0248", instTimeout[instIndex], 10 );

    //int instIndex = 16;
    //test_customIO( testOutputDir, instIndex, '0', "28346179", 0, 10 );
    //test_customIO( testOutputDir, instIndex, '0', "28346179", instTimeout[instIndex], 10 );

    //system( "pause" );
    return 0;
}