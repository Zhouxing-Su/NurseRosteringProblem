#include "Analysis.h"


using namespace std;


int main()
{
    //analyzeInstance();

    //validatorCheck( "log.csv", "checkResult.csv" );

    ValidatorArgvPack vap;
    rebuildSolution( "log.csv", "2015-03-30 Mon 15:55:26", "1", vap );
    callValidator( vap );

    return 0;
}