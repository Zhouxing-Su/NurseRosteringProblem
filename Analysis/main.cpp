#include "Analysis.h"


using namespace std;


int main()
{
    //analyzeInstance();

    //validatorCheck( "log.csv", "checkResult.csv" );

    //solutionAnalysis( "log.csv", "analysisResult.csv" );

    //analyzeCheckResult( "checkResult.csv", "statistic.csv" );

    ValidatorArgvPack vap;
    rebuildSolution( "log.csv", "2015-05-06 Wed 14:51:57", "0", vap );
    callValidator( vap );

    return 0;
}