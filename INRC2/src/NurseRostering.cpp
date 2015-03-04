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
    discoverNurseSkillRelation();

    sln.genInitSln_random();
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
    :Solver( i ), sln( *this )
{

}

NurseRostering::TabuSolver::~TabuSolver()
{

}

void NurseRostering::TabuSolver::discoverNurseSkillRelation()
{
    nurseNumOfSkill = vector<int>( scenario.skillTypeNum, 0 );
    nurseWithSkill = vector< vector<NurseID> >( scenario.nurseNum );

    for (NurseID n = 0; n < scenario.nurseNum; n++) {
        const vector<SkillID> &skills = scenario.nurses[n].skills;
        int skillNum = skills.size();
        for (int s = 0; s < skillNum; s++) {
            nurseNumOfSkill[skills[s]]++;
            nurseWithSkill[s].push_back( n );
        }
    }
}

NurseRostering::TabuSolver::Solution::Solution( TabuSolver &s )
    :solver( s ), assign( WEEKDAY_NUM, vector< vector< set<NurseID> > >( s.scenario.shiftTypeNum, vector< set<NurseID> >( s.scenario.skillTypeNum ) ) )
{

}

void NurseRostering::TabuSolver::Solution::genInitSln_random()
{
    for (int weekday = 0; weekday < WEEKDAY_NUM; weekday++) {
        // find out available nurses ( remove forbidden shift succession )
        vector< vector<NurseID> > availableNurse( solver.nurseWithSkill );

        // the less nurseNumOfSkill[skill] is, the smaller index in skillRank a skill will get
        std::vector<SkillID> skillRank;

        for (SkillID skill = 0; skill < solver.scenario.skillTypeNum; skill++) {
            for (ShiftID shift = 0; shift < solver.scenario.shiftTypeNum; shift++) {
                for (int i = 0; i < solver.getWeekData().minNurseNums[weekday][shift][skill]; i++) {
                    int nurse = rand() % availableNurse[skill].size();



                    // TODO


                    assign[weekday][shift][skill].insert( availableNurse[skill][nurse] );
                    // remove unavailable nurse
                    //store nurse set with certain skill?
                    // re-insert available nurse







                }
            }
        }
    }
}

bool NurseRostering::TabuSolver::Solution::isAvailableAssign( NurseID nurse, ShiftID shift, int weekday ) const
{
    if (weekday == 0) { // check history information
        return solver.scenario.shifts[solver.getHistory( nurse ).lastShift].illegalNextShifts[shift];
    } else {
        ;
    }

    return true;
}
