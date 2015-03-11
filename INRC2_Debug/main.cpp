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
    "-cusIn", "",
    "--cusOut", ""
};

const char *testOutputDir = "output/";
const std::string dir( "../INRC2/instance/" );
const std::vector<std::string> instance = {
    "n005w4", "n012w8", "n021w4", "n030w4", "n030w8",
    "n040w4", "n040w8", "n050w4", "n050w8", "n060w4", "n060w8",
    "n080w4", "n080w8", "n100w4", "n100w8", "n120w4", "n120w8"
};

const std::string scePrefix( "/Sc-" );
const std::string weekPrefix( "/WD-" );
const std::string initHisPrefix( "/H0-" );
const std::string hisPrefix( "history-week" );
const std::string solPrefix( "sol-week" );
const std::string fileSuffix( ".txt" );


int main()
{
    //analyzeInstance();
    test( testOutputDir, 0, '0', "1233", 10 );
    //test_customIO( testOutputDir, 0, '0', "1233", 10 );

    system( "pause" );
    return 0;
}