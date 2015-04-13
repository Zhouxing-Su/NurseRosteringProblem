#include "Analysis.h"


using namespace std;


int main()
{
    //analyzeInstance();

    //validatorCheck( "log.csv", "checkResult.csv" );

    //solutionAnalysis( "log.csv", "analysisResult.csv" );

    ValidatorArgvPack vap;
    rebuildSolution( "log.csv", "2015-04-11 Sat 23:32:12", "0", vap );
    callValidator( vap );

    return 0;
}