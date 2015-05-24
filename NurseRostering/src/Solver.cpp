#include "Solver.h"


using namespace std;


const Timer::Duration NurseRostering::Solver::SAVE_SOLUTION_TIME = Timer::Duration( 500 );
const double NurseRostering::Solver::INIT_PERTURB_STRENGTH = 0.2;
const double NurseRostering::Solver::PERTURB_STRENGTH_DELTA = 0.01;
const double NurseRostering::Solver::MAX_PERTURB_STRENGTH = 0.6;



NurseRostering::Solver::Solver( const NurseRostering &input, Timer::TimePoint st )
    : problem( input ), startTime( st ), timer( problem.timeout, startTime ), sln( *this ), dayTabuTenureBase( 1 ), shiftTabuTenureBase( 1 )
{
}


NurseRostering::ObjValue NurseRostering::Solver::checkFeasibility( const AssignTable &assign ) const
{
    ObjValue objValue = 0;
    NurseNumsOnSingleAssign nurseNum( countNurseNums( assign ) );

    // check H1: Single assignment per day
    // always true

    // check H2: Under-staffing
    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        for (ShiftID shift = NurseRostering::Scenario::Shift::ID_BEGIN; shift < problem.scenario.shiftSize; ++shift) {
            for (SkillID skill = NurseRostering::Scenario::Skill::ID_BEGIN; skill < problem.scenario.skillSize; ++skill) {
                if (nurseNum[weekday][shift][skill] < problem.weekData.minNurseNums[weekday][shift][skill]) {
                    objValue += DefaultPenalty::UnderStaff_Repair *
                        (problem.weekData.minNurseNums[weekday][shift][skill] - nurseNum[weekday][shift][skill]);
                }
            }
        }
    }

    // check H3: Shift type successions
    for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            objValue += DefaultPenalty::Succession_Repair *
                (!problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[assign[nurse][weekday].shift]);
        }
    }

    // check H4: Missing required skill
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            if (!problem.scenario.nurses[nurse].skills[assign[nurse][weekday].skill]) {
                return DefaultPenalty::FORBIDDEN_MOVE;
            }
        }
    }

    return objValue;
}

NurseRostering::ObjValue NurseRostering::Solver::checkFeasibility() const
{
    return checkFeasibility( optima.getAssignTable() );
}


NurseRostering::NurseNumsOnSingleAssign NurseRostering::Solver::countNurseNums( const AssignTable &assign ) const
{
    NurseNumsOnSingleAssign nurseNums( Weekday::SIZE,
        vector< vector<int> >( problem.scenario.shiftSize, vector<int>( problem.scenario.skillSize, 0 ) ) );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday <= Weekday::Sun; ++weekday) {
            ++nurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];
        }
    }

    return nurseNums;
}

void NurseRostering::Solver::discoverNurseSkillRelation()
{
    nurseNumOfSkill = vector<SkillID>( problem.scenario.skillSize, 0 );
    nurseWithSkill = vector< vector< vector<NurseID> > >( problem.scenario.skillSize );
    nursesHasSameSkill = vector< vector<bool> >( problem.scenario.nurseNum, vector<bool>( problem.scenario.nurseNum ) );

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        const vector<bool> &skills = problem.scenario.nurses[nurse].skills;
        unsigned skillNum = problem.scenario.nurses[nurse].skillNum;
        for (SkillID skill = NurseRostering::Scenario::Skill::ID_BEGIN; skill < problem.scenario.skillSize; ++skill) {
            if (skills[skill]) {
                ++nurseNumOfSkill[skill];
                if (skillNum > nurseWithSkill[skill].size()) {
                    nurseWithSkill[skill].resize( skillNum );
                }
                nurseWithSkill[skill][skillNum - 1].push_back( nurse );
            }
        }
        for (NurseID nurse2 = 0; nurse2 < problem.scenario.nurseNum; ++nurse2) {
            nursesHasSameSkill[nurse][nurse2] = problem.haveSameSkill( nurse, nurse2 );
        }
    }
}




void NurseRostering::Solver::init()
{
    iterationCount = 0;
    generationCount = 0;
    randGen.seed( problem.randSeed );

    setTabuTenure();
    setMaxNoImprove();

    discoverNurseSkillRelation();

    sln.genInitAssign();
    optima = sln;
}

void NurseRostering::Solver::solve()
{
    biasTabuSearch();
    //tabuSearch( MaxNoImproveForAllNeighborhood() );

#ifdef INRC2_LOG
    if (problem.history.currentWeek < problem.scenario.totalWeekNum) {
        sln.rebuild( optima );
        sln.evaluateObjValue( false );
        optima = Output( sln.getObjValue(), sln.getAssignTable(),
            optima.getSecondaryObjValue(), optima.getFindTime() );
    }
#endif
}

bool NurseRostering::Solver::updateOptima( const Output &localOptima )
{
    if (localOptima.getObjValue() < optima.getObjValue()) {
        optima = localOptima;
        return true;
    } else if (localOptima.getObjValue() == optima.getObjValue()) {
#ifdef INRC2_SECONDARY_OBJ_VALUE
        bool  isSelected = (localOptima.getSecondaryObjValue() < optima.getSecondaryObjValue());
#else
        bool isSelected = ((randGen() % 2) == 0);
#endif
        if (isSelected) {
            optima = localOptima;
            return true;
        }
    }

    return false;
}

NurseRostering::History NurseRostering::Solver::genHistory() const
{
    return Solution( *this, optima ).genHistory();
}

void NurseRostering::Solver::biasTabuSearch()
{
    while (!timer.isTimeOut()) {
        sln.tabuSearch_Rand( timer, MaxNoImproveForAllNeighborhood() );

        updateOptima( sln.getOptima() );
        const Output &output( (randGen() % BIAS_TS_ORIGIN_SELECT)
            ? optima : sln.getOptima() );
        sln.rebuild( output );

        sln.adjustWeightToBiasNurseWithGreaterPenalty( INVERSE_TOTAL_BIAS_RATIO, INVERSE_PENALTY_BIAS_RATIO );
        sln.tabuSearch_Rand( timer, MaxNoImproveForBiasTabuSearch() );
        ++generationCount;
        sln.rebuild( sln.getOptima() );
        sln.evaluateObjValue();
    }

    iterationCount = sln.getIterCount();
}

void NurseRostering::Solver::setTabuTenure()
{
    dayTabuTenureBase = 1 + Weekday::NUM;
    shiftTabuTenureBase = 1 + Weekday::NUM;

    dayTabuTenureAmp = 1 + dayTabuTenureBase / TABU_BASE_TO_AMP;
    shiftTabuTenureAmp = 1 + shiftTabuTenureBase / TABU_BASE_TO_AMP;
}
