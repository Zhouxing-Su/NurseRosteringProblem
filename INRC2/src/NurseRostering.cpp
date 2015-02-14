#include "NurseRostering.h"


using namespace std;


const int NurseRostering::WEEKDAY_NUM = 7;
const int NurseRostering::MAX_OBJ_VALUE = 2147483647; 

const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ShiftID_Any = -1;
const std::string NurseRostering::Scenario::Shift::ShiftName_Any( "Any" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ShiftID_None = -2;
const std::string NurseRostering::Scenario::Shift::ShiftName_None( "None" );

NurseRostering::NurseRostering()
{

}


NurseRostering::~NurseRostering()
{

}





NurseRostering::Solver::Solver( const Input &i, const std::string &an )
    : weekData( i.weekData ), scenario( i.scenario ), history( i.history ),
    solutionFileName( i.solutionFileName ), customOutputFileName( i.customOutputFileName ),
    randSeed( i.randSeed ), algorithmName( an )
{

}

NurseRostering::Solver::~Solver()
{

}

void NurseRostering::Solver::initResultSheet( std::ofstream &csvFile )
{
    csvFile << "Instance, Algorithm, RandSeed, Duration, IterCount, GenerationCount, ObjValue, Solution" << std::endl;
}

void NurseRostering::Solver::appendResultToSheet( const std::string &instanceFileName,
    std::ofstream &csvFile ) const
{
    if (check() != optima.objVal) {
        csvFile << "[LogicError] ";
    }

    csvFile << instanceFileName << ","
        << algorithmName << ","
        << randSeed << ","
        << (endTime - startTime) << ","
        << iterCount << ","
        << generationCount << ","
        << optima.objVal << ",";

    for (vector< vector<Assign> >::const_iterator iterd = optima.assigns.begin();
        iterd != optima.assigns.end(); iterd++) {
        for (vector<Assign>::const_iterator itern = iterd->begin();
            itern != iterd->end(); itern++) {
            csvFile << itern->shift << ' ' << itern->skill << ' ';
        }
    }

    csvFile << std::endl;
}

void NurseRostering::TabuSolver::init()
{

}

void NurseRostering::TabuSolver::solve()
{

}

int NurseRostering::TabuSolver::check() const
{
    return 0;
}

void NurseRostering::TabuSolver::print() const
{

}

NurseRostering::TabuSolver::TabuSolver( const Input &i, const std::string &an )
    :Solver( i, an )
{

}

NurseRostering::TabuSolver::~TabuSolver()
{

}
