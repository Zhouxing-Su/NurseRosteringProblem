#include "Analysis.h"


using namespace std;


int main()
{
    //analyzeInstance();

    //validatorCheck( "log.csv", "checkResult.csv" );

    //solutionAnalysis( "log.csv", "analysisResult.csv" );

    ValidatorArgvPack vap;
    rebuildSolution( "log.csv", "2015-04-01 Wed 10:10:40", "0", vap );
    callValidator( vap );

    return 0;
}