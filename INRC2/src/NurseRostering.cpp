#include "NurseRostering.h"


using namespace std;


const int NurseRostering::WEEKDAY_NUM = 7;
const int NurseRostering::MAX_OBJ_VALUE = 2147483647;

const int NurseRostering::Solver::ILLEGAL_SOLUTION = -1;

const NurseRostering::NurseID NurseRostering::Scenario::Nurse::ID_NONE = -1;

const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_ANY = -1;
const std::string NurseRostering::Scenario::Shift::NAME_ANY( "Any" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_NONE = -2;
const std::string NurseRostering::Scenario::Shift::NAME_NONE( "None" );

NurseRostering::NurseRostering()
{
    shiftMap[NurseRostering::Scenario::Shift::NAME_ANY] = NurseRostering::Scenario::Shift::ID_ANY;
    shiftMap[NurseRostering::Scenario::Shift::NAME_NONE] = NurseRostering::Scenario::Shift::ID_NONE;
}





NurseRostering::Solver::Solver( const NurseRostering &i ) : weekData( i.weekData ), scenario( i.scenario ), history( i.history ),
solutionFileName( i.solutionFileName ), customOutputFileName( i.customOutputFileName ),
randSeed( i.randSeed )
{

}

NurseRostering::Solver::~Solver()
{

}

int NurseRostering::Solver::check( const Assign &assgin ) const
{
    for (int weekday = 0; weekday < WEEKDAY_NUM; weekday++) {
        for (ShiftID shift = 0; shift < scenario.shiftTypeNum; shift++) {
            for (SkillID skill = 0; skill < scenario.skillTypeNum; skill++) {
                TODO
            }
        }
    }

    return ILLEGAL_SOLUTION;
}

int NurseRostering::Solver::check() const
{
    check( optima.assign );
}

void NurseRostering::Solver::print() const
{
    cout << optima.objVal << endl;
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

    bool feasible;
    do {
        feasible = sln.genInitSln_random();
    } while (feasible == false);

}

void NurseRostering::TabuSolver::solve()
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
    nurseWithSkill = vector< vector< vector<NurseID> > >( scenario.nurseNum );

    for (NurseID n = 0; n < scenario.nurseNum; n++) {
        const vector<SkillID> &skills = scenario.nurses[n].skills;
        int skillNum = skills.size();
        for (int s = 0; s < skillNum; s++) {
            nurseNumOfSkill[skills[s]]++;
            if (skillNum < nurseWithSkill[s].size()) {
                nurseWithSkill[s].resize( skillNum );
            }
            nurseWithSkill[s][skillNum - 1].push_back( n );
        }
    }
}

NurseRostering::TabuSolver::Solution::Solution( TabuSolver &s )
    :solver( s ), assign( s.scenario.nurseNum, vector< SingleAssign >( WEEKDAY_NUM ) )
{

}

bool NurseRostering::TabuSolver::Solution::genInitSln_random()
{
    AvailableNurses availableNurse( *this );

    for (int weekday = 0; weekday < WEEKDAY_NUM; weekday++) {
        // decide assign sequence of skill
        // the greater requiredNurseNum/nurseNumOfSkill[skill] is, the smaller index in skillRank a skill will get
        vector<SkillID> skillRank( solver.scenario.skillTypeNum );
        vector<double> dailyRequire( solver.scenario.skillTypeNum, 0 );
        for (SkillID skill = 0; skill < solver.scenario.skillTypeNum; skill++) {
            skillRank[skill] = skill;
            for (ShiftID shift = 0; shift < solver.scenario.shiftTypeNum; shift++) {
                dailyRequire[skill] += solver.getWeekData().minNurseNums[weekday][shift][skill];
            }
            dailyRequire[skill] /= solver.nurseNumOfSkill[skill];
        }

        class CmpDailyRequire
        {
        public:
            CmpDailyRequire( vector<double> &dr ) :dailyRequire( dr ) {}

            bool operator()( const SkillID &l, const SkillID &r )
            {
                return (dailyRequire[l] > dailyRequire[r]);
            }
        private:
            vector<double> &dailyRequire;
        }cmpDailyRequire( dailyRequire );
        sort( skillRank.begin(), skillRank.end(), cmpDailyRequire );

        // start assigning nurses
        for (int rank = 0; rank < solver.scenario.skillTypeNum; rank++) {
            SkillID skill = skillRank[rank];
            availableNurse.setEnvironment( weekday, skill );
            for (ShiftID shift = 0; shift < solver.scenario.shiftTypeNum; shift++) {
                availableNurse.setShift( shift );
                for (int i = 0; i < solver.getWeekData().minNurseNums[weekday][shift][skill]; i++) {
                    int nurse = availableNurse.getNurse();
                    if (nurse != NurseRostering::Scenario::Nurse::ID_NONE) {
                        assign[nurse][weekday] = SingleAssign( shift, skill );
                    } else {
                        cerr << "fail to generate feasible solution." << endl;
                        return false;
                    }
                }
            }
        }
    }

    solver.optima = NurseRostering::Solver::Output( objValue, assign );
    return true;
}

bool NurseRostering::TabuSolver::Solution::isValidAssign( NurseID nurse, ShiftID shift, int weekday ) const
{
    // check history information if it is the first day of week
    return (weekday == 0) ?
        solver.scenario.shifts[solver.getHistory( nurse ).lastShift].illegalNextShifts[shift]
        : solver.scenario.shifts[assign[nurse][weekday - 1].shift].illegalNextShifts[shift];
}



void NurseRostering::TabuSolver::Solution::AvailableNurses::setEnvironment( int w, SkillID s )
{
    weekday = w;
    skill = s;
    minSkillNum = 0;
    int size = nurseWithSkill[skill].size();
    validNurseNum_CurDay = vector<int>( size );
    validNurseNum_CurShift = vector<int>( size );
    for (int i = 0; i < size; i++) {
        validNurseNum_CurDay[i] = nurseWithSkill[skill][i].size();
        validNurseNum_CurShift[i] = validNurseNum_CurDay[i];
    }
}

void NurseRostering::TabuSolver::Solution::AvailableNurses::setShift( ShiftID s )
{
    shift = s;
    minSkillNum = 0;
    validNurseNum_CurShift = validNurseNum_CurDay;
}

NurseRostering::NurseID NurseRostering::TabuSolver::Solution::AvailableNurses::getNurse()
{
    // find nurses who have the required skill with minimum skill number
    while (true) {
        if (validNurseNum_CurShift[minSkillNum] > 0) {
            break;
        } else if (minSkillNum < validNurseNum_CurShift.size()) {
            minSkillNum++;
        } else {
            return NurseRostering::Scenario::Nurse::ID_NONE;
        }
    }

    // select one nurse from it
    while (true) {
        int n = rand() % validNurseNum_CurShift[minSkillNum];
        NurseID nurse = nurseWithSkill[skill][minSkillNum][n];
        vector<NurseID> &nurseSet = nurseWithSkill[skill][minSkillNum];
        if (sln.isValidAssign( nurse, shift, weekday )) {
            swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
            swap( nurseSet[validNurseNum_CurShift[minSkillNum]], --validNurseNum_CurDay[minSkillNum] );
            return nurse;
        } else {
            swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
        }
    }
}