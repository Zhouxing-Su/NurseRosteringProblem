#include "NurseRostering.h"


using namespace std;


const NurseRostering::ObjValue NurseRostering::MAX_OBJ_VALUE = 2000000000;
const std::string NurseRostering::TabuSolver::Name( "Tabu" );

const NurseRostering::NurseID NurseRostering::Scenario::Nurse::ID_NONE = -1;

const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_ANY = -1;
const std::string NurseRostering::Scenario::Shift::NAME_ANY( "Any" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_NONE = -2;
const std::string NurseRostering::Scenario::Shift::NAME_NONE( "None" );




NurseRostering::NurseRostering()
{
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_ANY] = NurseRostering::Scenario::Shift::ID_ANY;
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_NONE] = NurseRostering::Scenario::Shift::ID_NONE;
}




NurseRostering::Solver::Solver( const NurseRostering &input, const std::string &name, clock_t st )
    : problem( input ), algorithmName( name ), iterCount( 0 ), generationCount( 0 ),
    startTime( st ), endTime( st + input.timeout - SAVE_SOLUTION_TIME_IN_MILLISECOND )
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
    NurseNum nurseNum( countNurseNums( assign ) );

    // check H1: Single assignment per day
    // always true

    // check H2: Under-staffing
    for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
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
        if (assign.isAssigned( nurse, 0 )
            && (problem.history[nurse].lastShift != NurseRostering::Scenario::Shift::ID_NONE)) {
            if (!problem.scenario.shifts[problem.history[nurse].lastShift].legalNextShifts[assign[nurse][0].shift]) {
                return false;
            }
        }
    }
    for (int weekday = 1; weekday < Weekday::NUM; ++weekday) {
        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            if (assign.isAssigned( nurse, weekday ) && assign.isAssigned( nurse, weekday - 1 )) {
                if (!problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[assign[nurse][weekday].shift]) {
                    return false;
                }
            }
        }
    }

    // check H4: Missing required skill
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
            if (assign.isAssigned( nurse, weekday )) {
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
    NurseNum nurseNum( countNurseNums( assign ) );

    // check S1: Insufficient staffing for optimal coverage (30)
    for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
        for (ShiftID shift = 0; shift < problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < problem.scenario.skillTypeNum; ++skill) {
                int missingNurse = (problem.weekData.optNurseNums[weekday][shift][skill]
                    - nurseNum[weekday][shift][skill]);
                if (missingNurse > 0) {
                    objValue += Penalty::InsufficientStaff * missingNurse;
                }
            }
        }
    }

    // check S2: Consecutive assignments (15/30)
    // check S3: Consecutive days off (30)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        int consecutiveShift = problem.history[nurse].consecutiveShiftNum;
        int consecutiveDay = problem.history[nurse].consecutiveWorkingDayNum;
        int consecutiveDayOff = problem.history[nurse].consecutiveDayoffNum;
        bool shiftBegin = (consecutiveShift != 0);
        bool dayBegin = (consecutiveDay != 0);
        bool dayoffBegin = (consecutiveDayOff != 0);

        updateConsecutiveInfo( objValue, assign, nurse, Weekday::Mon, problem.history[nurse].lastShift,
            consecutiveShift, consecutiveDay, consecutiveDayOff,
            shiftBegin, dayBegin, dayoffBegin );

        for (int weekday = 1; weekday < Weekday::NUM; ++weekday) {
            updateConsecutiveInfo( objValue, assign, nurse, weekday, assign[nurse][weekday - 1].shift,
                consecutiveShift, consecutiveDay, consecutiveDayOff,
                shiftBegin, dayBegin, dayoffBegin );
        }
        // since penalty was calculated when switching assign, the penalty of last 
        // consecutive assignments are not considered. so finish it here.
        if (dayoffBegin && problem.history[nurse].consecutiveDayoffNum > problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveDayoffNum) {
            objValue += Penalty::ConsecutiveDayoff * Weekday::NUM;
        } else if (consecutiveDayOff > problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveDayoffNum) {
            objValue += Penalty::ConsecutiveDayoff *
                (consecutiveDayOff - problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveDayoffNum);
        } else if (consecutiveDayOff == 0) {    // working day
            if (shiftBegin && problem.history[nurse].consecutiveShiftNum > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += Penalty::ConsecutiveShift * Weekday::NUM;
            } else if (consecutiveShift > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += Penalty::ConsecutiveShift *
                    (consecutiveShift - problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum);
            }
            if (dayBegin && problem.history[nurse].consecutiveWorkingDayNum > problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveWorkingDayNum) {
                objValue += Penalty::ConsecutiveDay * Weekday::NUM;
            } else if (consecutiveDay > problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveWorkingDayNum) {
                objValue += Penalty::ConsecutiveDay *
                    (consecutiveDay - problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveWorkingDayNum);
            }
        }
    }

    // check S4: Preferences (10)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
            ShiftID shift = assign[nurse][weekday].shift;
            if (shift != NurseRostering::Scenario::Shift::ID_NONE) {
                objValue += Penalty::Preference *
                    problem.weekData.shiftOffs[weekday][shift][nurse];
            }
        }
    }

    // check S5: Complete weekend (30)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        objValue += Penalty::CompleteWeekend *
            (problem.scenario.contracts[problem.scenario.nurses[nurse].contract].completeWeekend
            && assign.isAssigned( nurse, Weekday::Sat )
            != assign.isAssigned( nurse, Weekday::Sun ));
    }

    // check S6: Total assignments (20)
    // check S7: Total working weekends (30)
    if (problem.weekCount < problem.scenario.maxWeekCount) {





    } else {    // final week
        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            int assignNum = problem.history[nurse].shiftNum;
            for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
                assignNum += assign.isAssigned( nurse, weekday );
            }
            objValue += Penalty::TotalAssign * distanceToRange( assignNum,
                problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minShiftNum,
                problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxShiftNum );

            int workingWeekend = problem.history[nurse].workingWeekendNum +
                (assign.isAssigned( nurse, Weekday::Sat ) || assign.isAssigned( nurse, Weekday::Sun ));
            // calculate exceeding weekends
            workingWeekend -= problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
            if (workingWeekend > 0) {
                objValue += Penalty::TotalWorkingWeekend * workingWeekend;
            }
        }
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
        << (optima.findTime - startTime) << "ms,"
        << iterCount << ","
        << generationCount << ","
        << optima.objVal << ",";

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
            if (optima.assign.isAssigned( nurse, weekday )) {
                csvFile << optima.assign[nurse][weekday].shift << ' '
                    << optima.assign[nurse][weekday].skill << ' ';
            }
        }
    }


    csvFile << endl;
    csvFile.close();
}

NurseRostering::NurseNum NurseRostering::Solver::countNurseNums( const Assign &assign ) const
{
    NurseNum nurseNums( Weekday::NUM,
        vector< vector<int> >( problem.scenario.shiftTypeNum, vector<int>( problem.scenario.skillTypeNum, 0 ) ) );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
            if (assign.isAssigned( nurse, weekday )) {
                ++nurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];
            }
        }
    }

    return nurseNums;
}

void NurseRostering::Solver::updateConsecutiveInfo_problemOnBorder( int &objValue,
    const Assign &assign, NurseID nurse, int weekday, ShiftID lastShift,
    int &consecutiveShift, int &consecutiveDay, int &consecutiveDayOff ) const
{
    ShiftID shift = assign[nurse][weekday].shift;
    if (shift != NurseRostering::Scenario::Shift::ID_NONE) {
        if (shift == lastShift) {
            ++consecutiveShift;
        } else if (lastShift != NurseRostering::Scenario::Shift::ID_NONE) { // another shift
            int missingShift = distanceToRange( consecutiveShift,
                problem.scenario.shifts[lastShift].minConsecutiveShiftNum,
                problem.scenario.shifts[lastShift].maxConsecutiveShiftNum );
            objValue += Penalty::ConsecutiveShift * missingShift;
            consecutiveShift = 1;
        }
        if (consecutiveDay == 0) {  // switch from consecutive day off to working
            int missingDayOff = distanceToRange( consecutiveDayOff,
                problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minConsecutiveDayoffNum,
                problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveDayoffNum );
            objValue += Penalty::ConsecutiveDayoff * missingDayOff;
        }
        ++consecutiveDay;
        consecutiveDayOff = 0;
    } else {    // day off
        if (consecutiveDayOff == 0) {   // switch from consecutive working to day off
            int missingShift = distanceToRange( consecutiveShift,
                problem.scenario.shifts[lastShift].minConsecutiveShiftNum,
                problem.scenario.shifts[lastShift].maxConsecutiveShiftNum );
            int missingDay = distanceToRange( consecutiveDay,
                problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minConsecutiveWorkingDayNum,
                problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveWorkingDayNum );
            objValue += Penalty::ConsecutiveDay * missingShift;
            objValue += Penalty::ConsecutiveShift * missingDay;
        }
        consecutiveShift = 0;
        consecutiveDay = 0;
        ++consecutiveDayOff;
    }
}

void NurseRostering::Solver::updateConsecutiveInfo( int &objValue,
    const Assign &assign, NurseID nurse, int weekday, ShiftID lastShift,
    int &consecutiveShift, int &consecutiveDay, int &consecutiveDayOff,
    bool &shiftBegin, bool &dayBegin, bool &dayoffBegin ) const
{
    ShiftID shift = assign[nurse][weekday].shift;
    if (shift != NurseRostering::Scenario::Shift::ID_NONE) {    // working day
        if (consecutiveDay == 0) {  // switch from consecutive day off to working
            if (dayoffBegin) {
                if (problem.history[nurse].consecutiveDayoffNum > problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveDayoffNum) {
                    objValue += Penalty::ConsecutiveDayoff * weekday;
                } else {
                    objValue += Penalty::ConsecutiveDayoff * distanceToRange( consecutiveDayOff,
                        problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minConsecutiveDayoffNum,
                        problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveDayoffNum );
                }
                dayoffBegin = false;
            } else {
                objValue += Penalty::ConsecutiveDayoff * distanceToRange( consecutiveDayOff,
                    problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minConsecutiveDayoffNum,
                    problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveDayoffNum );
            }
            consecutiveDayOff = 0;
            consecutiveShift = 1;
        } else {    // keep working
            if (shift == lastShift) {
                ++consecutiveShift;
            } else { // another shift
                if (shiftBegin) {
                    if (problem.history[nurse].consecutiveShiftNum > problem.scenario.shifts[lastShift].maxConsecutiveShiftNum) {
                        objValue += Penalty::ConsecutiveShift * weekday;
                    } else {
                        objValue += Penalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
                            problem.scenario.shifts[lastShift].minConsecutiveShiftNum,
                            problem.scenario.shifts[lastShift].maxConsecutiveShiftNum );
                    }
                    shiftBegin = false;
                } else {
                    objValue += Penalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
                        problem.scenario.shifts[lastShift].minConsecutiveShiftNum,
                        problem.scenario.shifts[lastShift].maxConsecutiveShiftNum );
                }
                consecutiveShift = 1;
            }
        }
        ++consecutiveDay;
    } else {    // day off
        if (consecutiveDayOff == 0) {   // switch from consecutive working to day off
            if (shiftBegin) {
                if (problem.history[nurse].consecutiveShiftNum > problem.scenario.shifts[lastShift].maxConsecutiveShiftNum) {
                    objValue += Penalty::ConsecutiveShift * weekday;
                } else {
                    objValue += Penalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                        problem.scenario.shifts[lastShift].minConsecutiveShiftNum,
                        problem.scenario.shifts[lastShift].maxConsecutiveShiftNum );
                }
                shiftBegin = false;
            } else {
                objValue += Penalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                    problem.scenario.shifts[lastShift].minConsecutiveShiftNum,
                    problem.scenario.shifts[lastShift].maxConsecutiveShiftNum );
            }
            if (dayBegin) {
                if (problem.history[nurse].consecutiveWorkingDayNum > problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveWorkingDayNum) {
                    objValue += Penalty::ConsecutiveDay * weekday;
                } else {
                    objValue += Penalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                        problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minConsecutiveWorkingDayNum,
                        problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveWorkingDayNum );
                }
                dayBegin = false;
            } else {
                objValue += Penalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                    problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minConsecutiveWorkingDayNum,
                    problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxConsecutiveWorkingDayNum );
            }
            consecutiveShift = 0;
            consecutiveDay = 0;
        }
        ++consecutiveDayOff;
    }
}




void NurseRostering::TabuSolver::init()
{
    // TODO
    srand( problem.randSeed );

    initAssistData();

    if (sln.initAssign() == false) {
        sln.repair();
    }

    sln.initObjValue();
    sln.initAssistData();

    optima = NurseRostering::Solver::Output( sln );
}

void NurseRostering::TabuSolver::solve()
{
    // TODO
    while (true) {
        // check time
        ++iterCount;
        if (!(iterCount & CHECK_TIME_INTERVAL_MASK_IN_ITER)) {
            if (clock() >= endTime) {
                break;
            }
        }

        // 

    }
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

void NurseRostering::TabuSolver::initAssistData()
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
    :solver( s ), assign( s.problem.scenario.nurseNum, Weekday::NUM )
{

}


bool NurseRostering::TabuSolver::Solution::initAssign()
{
    AvailableNurses availableNurse( *this );

    for (int weekday = Weekday::Mon; weekday < Weekday::NUM; ++weekday) {
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

    return true;
}

void NurseRostering::TabuSolver::Solution::resetAssign()
{
    assign = Assign( solver.problem.scenario.nurseNum, Weekday::NUM );
}


void NurseRostering::TabuSolver::Solution::initAssistData()
{
    // TODO

}

void NurseRostering::TabuSolver::Solution::initObjValue()
{
    // TODO
    objValue = solver.checkObjValue( assign );
}

void NurseRostering::TabuSolver::Solution::repair()
{
    // TODO
    while (true) {
        bool feasible = initAssign();
        if (feasible) {
            break;
        } else {
            resetAssign();
        }
    }
}

bool NurseRostering::TabuSolver::Solution::isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const
{
    // check history information if it is the first day of week
    if (weekday == 0) {
        return ((solver.problem.history[nurse].lastShift == NurseRostering::Scenario::Shift::ID_NONE)
            || solver.problem.scenario.shifts[solver.problem.history[nurse].lastShift].legalNextShifts[shift]);
    } else {
        return (!assign.isAssigned( nurse, weekday - 1 )
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
            if (sln.getAssign().isAssigned( nurse, weekday )) { // set the nurse invalid for current day
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
