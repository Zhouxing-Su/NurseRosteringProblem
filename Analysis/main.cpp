#include "Analysis.h"


using namespace std;


int main()
{
    //analyzeInstance();

    //validatorCheck( "log.csv", "checkResult.csv" );

    ValidatorArgvPack vap;
    rebuildSolution( "log.csv", "2015-04-02 Thu 10:40:01", "0", vap );
    callValidator( vap );

    return 0;
}