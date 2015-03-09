#include "NurseRostering.h"


using namespace std;


const std::string NurseRostering::TabuSolver::Name( "Tabu" );

const std::string NurseRostering::Scenario::Shift::NAME_ANY( "Any" );
const std::string NurseRostering::Scenario::Shift::NAME_NONE( "None" );




NurseRostering::NurseRostering()
{
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_ANY] = NurseRostering::Scenario::Shift::ID_ANY;
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_NONE] = NurseRostering::Scenario::Shift::ID_NONE;
}





NurseRostering::Solver::Solver( const NurseRostering &input, const std::string &name, clock_t st )
    : problem( input ), algorithmName( name ), iterCount( 0 ), generationCount( 0 ), startTime( st )
{
}

NurseRostering::Solver::~Solver()
{

}

bool NurseRostering::Solver::check() const
{
    return (checkFeasibility() && (checkObjValue() == optima.objVal));
}

bool NurseRostering::Solver::checkFeasibility( const Assign &assign ) const
{
    NurseNum_Day_Shift_Skill nurseNum( countNurseNums( assign ) );

    // check H1: Single assignment per day
    // always true

    // check H2: Under-staffing
    for (int weekday = 0; weekday < WEEKDAY_NUM; ++weekday) {
        for (ShiftID shift = 0; shift < problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < problem.scenario.skillTypeNum; ++skill) {
                if (nurseNum[weekday][shift][skill] < problem.weekData.minNurseNums[weekday][shift][skill]) {
                    return false;
                }
            }
        }
    }

    // check H3: Shift type successions
    // first day check the history
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        if ((assign[nurse][0].shift != NurseRostering::Scenario::Shift::ID_NONE)
            && (problem.history[nurse].lastShift != NurseRostering::Scenario::Shift::ID_NONE)) {
            if (!problem.scenario.shifts[problem.history[nurse].lastShift].legalNextShifts[assign[nurse][0].shift]) {
                return false;
            }
        }
    }
    for (int weekday = 1; weekday < WEEKDAY_NUM; ++weekday) {
        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            if ((assign[nurse][weekday].shift != NurseRostering::Scenario::Shift::ID_NONE)
                && (assign[nurse][weekday - 1].shift != NurseRostering::Scenario::Shift::ID_NONE)) {
                if (!problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[assign[nurse][weekday].shift]) {
                    return false;
                }
            }
        }
    }

    // check H4: Missing required skill
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = 0; weekday < WEEKDAY_NUM; ++weekday) {
            if (assign[nurse][weekday].shift != NurseRostering::Scenario::Shift::ID_NONE) {
                const vector<SkillID> &skills( problem.scenario.nurses[nurse].skills );
                if (find( skills.begin(), skills.end(),
                    assign[nurse][weekday].skill ) == skills.end()) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool NurseRostering::Solver::checkFeasibility() const
{
    return checkFeasibility( optima.assign );
}

int NurseRostering::Solver::checkObjValue( const Assign &assign ) const
{
    ObjValue objValue = 0;
    NurseNum_Day_Shift_Skill nurseNum( countNurseNums( assign ) );

    // check S1: Insufficient staffing for optimal coverage (30)


    // check S2: Consecutive assignments (15/30)


    // check S3: Consecutive days off (30)


    // check S4: Preferences (10)


    // check S5: Complete weekend (30)


    // check S6: Total assignments (20)
    // check S7: Total working weekends (30)
    if (problem.weekCount < problem.scenario.maxWeekCount) {

    } else {

    }

    return objValue;
}

int NurseRostering::Solver::checkObjValue() const
{
    return checkObjValue( optima.assign );
}

void NurseRostering::Solver::print() const
{
    cout << optima.objVal << endl;
}

void NurseRostering::Solver::initResultSheet( std::ofstream &csvFile )
{
    csvFile << "Time, Instance, Algorithm, RandSeed, Duration, IterCount, GenerationCount, ObjValue, Solution" << std::endl;
}

void NurseRostering::Solver::record( const std::string logFileName, const std::string &instanceName ) const
{
    ofstream csvFile( logFileName, ios::app );
    csvFile.seekp( 0, ios::beg );
    ios::pos_type begin = csvFile.tellp();
    csvFile.seekp( 0, ios::end );
    if (csvFile.tellp() == begin) {
        initResultSheet( csvFile );
    }

    if (!check()) {
        csvFile << "[LogicError] ";
    }

    char timeBuf[64];
    time_t t = time( NULL );
    tm *date = localtime( &t );
    strftime( timeBuf, 64, "%Y-%m-%d %a %H:%M:%S", date );

    csvFile << timeBuf << ","
        << instanceName << ","
        << algorithmName << ","
        << problem.randSeed << ","
        << (endTime - startTime) << "ms,"
        << iterCount << ","
        << generationCount << ","
        << optima.objVal << ",";

    for (NurseRostering::NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = 0; weekday < NurseRostering::WEEKDAY_NUM; ++weekday) {
            if (optima.assign[nurse][weekday].shift != NurseRostering::Scenario::Shift::ID_NONE) {
                csvFile << optima.assign[nurse][weekday].shift << ' '
                    << optima.assign[nurse][weekday].skill << ' ';
            }
        }
    }


    csvFile << endl;
    csvFile.close();
}

NurseRostering::NurseNum_Day_Shift_Skill NurseRostering::Solver::countNurseNums( const Assign &assign ) const
{
    NurseNum_Day_Shift_Skill nurseNums( WEEKDAY_NUM,
        vector< vector<int> >( problem.scenario.shiftTypeNum, vector<int>( problem.scenario.skillTypeNum, 0 ) ) );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = 0; weekday < WEEKDAY_NUM; ++weekday) {
            if (assign[nurse][weekday].shift != NurseRostering::Scenario::Shift::ID_NONE) {
                ++nurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];
            }
        }
    }

    return nurseNums;
}




void NurseRostering::TabuSolver::init()
{
    discoverNurseSkillRelation();

    while (true) {
        bool feasible = sln.genInitSln_random();
        if (feasible == true) {
            break;
        } else {
            sln.resetAssign();
        }
    }
    optima = NurseRostering::Solver::Output( sln );

}

void NurseRostering::TabuSolver::solve()
{


    endTime = clock();
}

NurseRostering::TabuSolver::TabuSolver( const NurseRostering &i, clock_t st )
    :Solver( i, Name, st ), sln( *this ),
    nurseNumOfSkill( vector<int>( i.scenario.skillTypeNum, 0 ) ),
    nurseWithSkill( vector< vector< vector<NurseID> > >( problem.scenario.skillTypeNum ) )
{

}

NurseRostering::TabuSolver::~TabuSolver()
{

}

void NurseRostering::TabuSolver::discoverNurseSkillRelation()
{
    for (NurseID n = 0; n < problem.scenario.nurseNum; ++n) {
        const vector<SkillID> &skills = problem.scenario.nurses[n].skills;
        unsigned skillNum = skills.size();
        for (unsigned s = 0; s < skillNum; ++s) {
            SkillID skill = skills[s];
            ++nurseNumOfSkill[skill];
            if (skillNum > nurseWithSkill[skill].size()) {
                nurseWithSkill[skill].resize( skillNum );
            }
            nurseWithSkill[skill][skillNum - 1].push_back( n );
        }
    }
}






NurseRostering::TabuSolver::Solution::Solution( TabuSolver &s )
    :solver( s ), assign( s.problem.scenario.nurseNum, vector< SingleAssign >( WEEKDAY_NUM ) )
{

}

void NurseRostering::TabuSolver::Solution::resetAssign()
{
    assign = Assign( solver.problem.scenario.nurseNum, vector< SingleAssign >( WEEKDAY_NUM ) );
}


bool NurseRostering::TabuSolver::Solution::genInitSln_random()
{
    AvailableNurses availableNurse( *this );

    for (int weekday = 0; weekday < WEEKDAY_NUM; ++weekday) {
        // decide assign sequence of skill
        // the greater requiredNurseNum/nurseNumOfSkill[skill] is, the smaller index in skillRank a skill will get
        vector<SkillID> skillRank( solver.problem.scenario.skillTypeNum );
        vector<double> dailyRequire( solver.problem.scenario.skillTypeNum, 0 );
        for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
            skillRank[skill] = skill;
            for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                dailyRequire[skill] += solver.problem.weekData.minNurseNums[weekday][shift][skill];
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
        for (int rank = 0; rank < solver.problem.scenario.skillTypeNum; ++rank) {
            SkillID skill = skillRank[rank];
            availableNurse.setEnvironment( weekday, skill );
            for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
                availableNurse.setShift( shift );
                for (int i = 0; i < solver.problem.weekData.minNurseNums[weekday][shift][skill]; ++i) {
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

    objValue = solver.checkObjValue( assign );
    return true;
}

void NurseRostering::TabuSolver::Solution::genNewHistory()
{

}

bool NurseRostering::TabuSolver::Solution::isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const
{
    // check history information if it is the first day of week
    if (weekday == 0) {
        return ((solver.problem.history[nurse].lastShift == NurseRostering::Scenario::Shift::ID_NONE)
            || solver.problem.scenario.shifts[solver.problem.history[nurse].lastShift].legalNextShifts[shift]);
    } else {
        return ((assign[nurse][weekday - 1].shift == NurseRostering::Scenario::Shift::ID_NONE)
            || solver.problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[shift]);
    }
}


void NurseRostering::TabuSolver::Solution::AvailableNurses::setEnvironment( int w, SkillID s )
{
    weekday = w;
    skill = s;
    minSkillNum = 0;
    int size = nurseWithSkill[skill].size();
    validNurseNum_CurDay = vector<int>( size );
    validNurseNum_CurShift = vector<int>( size );
    for (int i = 0; i < size; ++i) {
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
    while (true) {
        // find nurses who have the required skill with minimum skill number
        while (true) {
            if (validNurseNum_CurShift[minSkillNum] > 0) {
                break;
            } else if (++minSkillNum == validNurseNum_CurShift.size()) {
                return NurseRostering::Scenario::Nurse::ID_NONE;
            }
        }

        // select one nurse from it
        while (true) {
            int n = rand() % validNurseNum_CurShift[minSkillNum];
            NurseID nurse = nurseWithSkill[skill][minSkillNum][n];
            vector<NurseID> &nurseSet = nurseWithSkill[skill][minSkillNum];
            if (sln.isAssigned( nurse, weekday )) { // set the nurse invalid for current day
                swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
                swap( nurseSet[validNurseNum_CurShift[minSkillNum]], nurseSet[--validNurseNum_CurDay[minSkillNum]] );
            } else if (sln.isValidSuccession( nurse, shift, weekday )) {
                swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
                swap( nurseSet[validNurseNum_CurShift[minSkillNum]], nurseSet[--validNurseNum_CurDay[minSkillNum]] );
                return nurse;
            } else {    // set the nurse invalid for current shift
                swap( nurseSet[n], nurseSet[--validNurseNum_CurShift[minSkillNum]] );
            }
            if (validNurseNum_CurShift[minSkillNum] == 0) {
                break;
            }
        }
    }
}