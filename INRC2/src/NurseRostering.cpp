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
    shiftMap[NurseRostering::Scenario::Shift::ShiftName_Any] = NurseRostering::Scenario::Shift::ShiftID_Any;
    shiftMap[NurseRostering::Scenario::Shift::ShiftName_None] = NurseRostering::Scenario::Shift::ShiftID_None;
}





NurseRostering::Solver::Solver( const NurseRostering &i ) : weekData( i.weekData ), scenario( i.scenario ), history( i.history ),
solutionFileName( i.solutionFileName ), customOutputFileName( i.customOutputFileName ),
randSeed( i.randSeed )
{

}

NurseRostering::Solver::~Solver()
{

}

void NurseRostering::Solver::initResultSheet( std::ofstream &csvFile )
{
    csvFile << "Instance, Algorithm, RandSeed, Duration, IterCount, GenerationCount, ObjValue, Solution" << std::endl;
}

void NurseRostering::Solver::appendResultToSheet( const std::string &instanceName,
    const std::string logFileName ) const
{
    ofstream csvFile( logFileName );
    ios::pos_type begin = csvFile.tellp();
    csvFile.seekp( 0, ios::end );
    if (csvFile.tellp() == begin) {
        initResultSheet( csvFile );
    }

    if (check() != optima.objVal) {
        csvFile << "[LogicError] ";
    }

    csvFile << instanceName << ","
        << algorithmName << ","
        << randSeed << ","
        << (endTime - startTime) << ","
        << iterCount << ","
        << generationCount << ","
        << optima.objVal << ",";

    // leave out solution, check it in solution files

    csvFile << endl;
    csvFile.close();
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

void NurseRostering::TabuSolver::record() const
{

}

NurseRostering::TabuSolver::TabuSolver( const NurseRostering &i )
    :Solver( i ), sln( *this, i.scenario.skillNames.size(), i.scenario.shifts.size() )
{

}

NurseRostering::TabuSolver::~TabuSolver()
{

}

NurseRostering::TabuSolver::Solution::Solution( TabuSolver &s, int skillNum, int shiftNum )
    :solver( s )
{

}

void NurseRostering::TabuSolver::Solution::genInitSln_random()
{
    int availableNurseNum = solver.scenario.nurseNum;
    vector<NurseID> availableNurse( availableNurseNum );
    for (int i = 0; i < solver.scenario.nurseNum; i++) {
        availableNurse[i] = i;
    }

    for (int weekday = 0; weekday < WEEKDAY_NUM; weekday++) {
        for (ShiftID shift = 0; shift < solver.scenario.shiftTypeNum; shift++) {
            for (Skill skill = 0; skill < solver.scenario.skillTypeNum; skill++) {
                for (int i = 0; i < solver.getWeekData().minNurseNums[weekday][shift][skill]; i++) {
                    int nurse = rand() % availableNurseNum;
                    assign[weekday][shift][skill].insert( availableNurse[nurse] );
                    // remove unavailable nurse
                    //store nurse set with certain skill?
                    // re-insert available nurse
                }
            }
        }
    }
}
