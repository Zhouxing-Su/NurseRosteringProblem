#include "NurseRostering.h"


using namespace std;


const NurseRostering::ObjValue NurseRostering::MAX_OBJ_VALUE = 1000000000;
const std::string NurseRostering::TabuSolver::Name( "Tabu" );

const NurseRostering::NurseID NurseRostering::Scenario::Nurse::ID_NONE = -1;

const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_ANY = -2;
const std::string NurseRostering::Scenario::Shift::NAME_ANY( "Any" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_NONE = -1;
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
    NurseNumsOnSingleAssign nurseNum( countNurseNums( assign ) );

    // check H1: Single assignment per day
    // always true

    // check H2: Under-staffing
    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
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
        if (assign.isWorking( nurse, Weekday::Mon )
            && (problem.history.lastShifts[nurse] != NurseRostering::Scenario::Shift::ID_NONE)) {
            if (!problem.scenario.shifts[problem.history.lastShifts[nurse]].legalNextShifts[assign[nurse][Weekday::Mon].shift]) {
                return false;
            }
        }
    }
    for (int weekday = Weekday::Tue; weekday < Weekday::SIZE; ++weekday) {
        for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
            if (assign.isWorking( nurse, weekday ) && assign.isWorking( nurse, weekday - 1 )) {
                if (!problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[assign[nurse][weekday].shift]) {
                    return false;
                }
            }
        }
    }

    // check H4: Missing required skill
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
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
    NurseNumsOnSingleAssign nurseNums( countNurseNums( assign ) );

    // check S1: Insufficient staffing for optimal coverage (30)
    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
        for (ShiftID shift = 0; shift < problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < problem.scenario.skillTypeNum; ++skill) {
                int missingNurse = (problem.weekData.optNurseNums[weekday][shift][skill]
                    - nurseNums[weekday][shift][skill]);
                if (missingNurse > 0) {
                    objValue += Penalty::InsufficientStaff * missingNurse;
                }
            }
        }
    }

    // check S2: Consecutive assignments (15/30)
    // check S3: Consecutive days off (30)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        int consecutiveShift = problem.history.consecutiveShiftNums[nurse];
        int consecutiveDay = problem.history.consecutiveDayNums[nurse];
        int consecutiveDayOff = problem.history.consecutiveDayoffNums[nurse];
        bool shiftBegin = (consecutiveShift != 0);
        bool dayBegin = (consecutiveDay != 0);
        bool dayoffBegin = (consecutiveDayOff != 0);

        checkConsecutiveViolation( objValue, assign, nurse, Weekday::Mon, problem.history.lastShifts[nurse],
            consecutiveShift, consecutiveDay, consecutiveDayOff,
            shiftBegin, dayBegin, dayoffBegin );

        for (int weekday = Weekday::Tue; weekday < Weekday::SIZE; ++weekday) {
            checkConsecutiveViolation( objValue, assign, nurse, weekday, assign[nurse][weekday - 1].shift,
                consecutiveShift, consecutiveDay, consecutiveDayOff,
                shiftBegin, dayBegin, dayoffBegin );
        }
        // since penalty was calculated when switching assign, the penalty of last 
        // consecutive assignments are not considered. so finish it here.
        const ContractID &contractID( problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
        if (dayoffBegin && problem.history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
            objValue += Penalty::ConsecutiveDayOff * Weekday::NUM;
        } else if (consecutiveDayOff > contract.maxConsecutiveDayoffNum) {
            objValue += Penalty::ConsecutiveDayOff *
                (consecutiveDayOff - contract.maxConsecutiveDayoffNum);
        } else if (consecutiveDayOff == 0) {    // working day
            if (shiftBegin && problem.history.consecutiveShiftNums[nurse] > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += Penalty::ConsecutiveShift * Weekday::NUM;
            } else if (consecutiveShift > problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum) {
                objValue += Penalty::ConsecutiveShift *
                    (consecutiveShift - problem.scenario.shifts[assign[nurse][Weekday::Sun].shift].maxConsecutiveShiftNum);
            }
            if (dayBegin && problem.history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                objValue += Penalty::ConsecutiveDay * Weekday::NUM;
            } else if (consecutiveDay > contract.maxConsecutiveDayNum) {
                objValue += Penalty::ConsecutiveDay *
                    (consecutiveDay - contract.maxConsecutiveDayNum);
            }
        }
    }

    // check S4: Preferences (10)
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
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
            && (assign.isWorking( nurse, Weekday::Sat )
            != assign.isWorking( nurse, Weekday::Sun )));
    }

    // check S6: Total assignments (20)
    // check S7: Total working weekends (30)
    int totalWeekNum = problem.scenario.totalWeekNum;
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        int min = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].minShiftNum;
        int max = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxShiftNum;
        int assignNum = problem.history.totalAssignNums[nurse];
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            assignNum += assign.isWorking( nurse, weekday );
        }
        objValue += Penalty::TotalAssign * distanceToRange( assignNum * totalWeekNum,
            min * problem.history.currentWeek, max * problem.history.currentWeek ) / totalWeekNum;

        int maxWeekend = problem.scenario.contracts[problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
        int historyWeekend = problem.history.totalWorkingWeekendNums[nurse] * totalWeekNum;
        int exceedingWeekend = historyWeekend - (maxWeekend * problem.history.currentWeek) +
            ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * totalWeekNum);
        if (exceedingWeekend > 0) {
            objValue += Penalty::TotalWorkingWeekend * exceedingWeekend / totalWeekNum;
        }

        // remove penalty in the history except the first week
        if (problem.history.pastWeekCount > 0) {
            objValue -= Penalty::TotalAssign * distanceToRange(
                problem.history.totalAssignNums[nurse] * totalWeekNum,
                min * problem.history.pastWeekCount, max * problem.history.pastWeekCount ) / totalWeekNum;

            historyWeekend -= maxWeekend * problem.history.pastWeekCount;
            if (historyWeekend > 0) {
                objValue -= Penalty::TotalWorkingWeekend * historyWeekend / totalWeekNum;
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
    cout << (optima.objVal / Penalty::AMP) << endl;
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
        << (optima.objVal / static_cast<double>(Penalty::AMP)) << ",";

    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (optima.assign.isWorking( nurse, weekday )) {
                csvFile << optima.assign[nurse][weekday].shift << ' '
                    << optima.assign[nurse][weekday].skill << ' ';
            }
        }
    }


    csvFile << endl;
    csvFile.close();
}

NurseRostering::NurseNumsOnSingleAssign NurseRostering::Solver::countNurseNums( const Assign &assign ) const
{
    NurseNumsOnSingleAssign nurseNums( Weekday::SIZE,
        vector< vector<int> >( problem.scenario.shiftTypeNum, vector<int>( problem.scenario.skillTypeNum, 0 ) ) );
    for (NurseID nurse = 0; nurse < problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            if (assign.isWorking( nurse, weekday )) {
                ++nurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];
            }
        }
    }

    return nurseNums;
}

void NurseRostering::Solver::checkConsecutiveViolation( int &objValue,
    const Assign &assign, NurseID nurse, int weekday, ShiftID lastShiftID,
    int &consecutiveShift, int &consecutiveDay, int &consecutiveDayOff,
    bool &shiftBegin, bool &dayBegin, bool &dayoffBegin ) const
{
    const ContractID &contractID = problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( problem.scenario.contracts[contractID] );
    const ShiftID &shift = assign[nurse][weekday].shift;
    if (shift != NurseRostering::Scenario::Shift::ID_NONE) {    // working day
        if (consecutiveDay == 0) {  // switch from consecutive day off to working
            if (dayoffBegin) {
                if (problem.history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                    objValue += Penalty::ConsecutiveDayOff * (weekday - Weekday::Mon);
                } else {
                    objValue += Penalty::ConsecutiveDayOff * distanceToRange( consecutiveDayOff,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
                dayoffBegin = false;
            } else {
                objValue += Penalty::ConsecutiveDayOff * distanceToRange( consecutiveDayOff,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            }
            consecutiveDayOff = 0;
            consecutiveShift = 1;
        } else {    // keep working
            if (shift == lastShiftID) {
                ++consecutiveShift;
            } else { // another shift
                const Scenario::Shift &lastShift( problem.scenario.shifts[lastShiftID] );
                if (shiftBegin) {
                    if (problem.history.consecutiveShiftNums[nurse] > lastShift.maxConsecutiveShiftNum) {
                        objValue += Penalty::ConsecutiveShift * (weekday - Weekday::Mon);
                    } else {
                        objValue += Penalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
                            lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                    }
                    shiftBegin = false;
                } else {
                    objValue += Penalty::ConsecutiveShift *  distanceToRange( consecutiveShift,
                        lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                }
                consecutiveShift = 1;
            }
        }
        ++consecutiveDay;
    } else {    // day off
        if (consecutiveDayOff == 0) {   // switch from consecutive working to day off
            const Scenario::Shift &lastShift( problem.scenario.shifts[lastShiftID] );
            if (shiftBegin) {
                if (problem.history.consecutiveShiftNums[nurse] > lastShift.maxConsecutiveShiftNum) {
                    objValue += Penalty::ConsecutiveShift * (weekday - Weekday::Mon);
                } else {
                    objValue += Penalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                        lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
                }
                shiftBegin = false;
            } else {
                objValue += Penalty::ConsecutiveShift * distanceToRange( consecutiveShift,
                    lastShift.minConsecutiveShiftNum, lastShift.maxConsecutiveShiftNum );
            }
            if (dayBegin) {
                if (problem.history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                    objValue += Penalty::ConsecutiveDay * (weekday - Weekday::Mon);
                } else {
                    objValue += Penalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
                dayBegin = false;
            } else {
                objValue += Penalty::ConsecutiveDay * distanceToRange( consecutiveDay,
                    contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            }
            consecutiveShift = 0;
            consecutiveDay = 0;
        }
        ++consecutiveDayOff;
    }
}




void NurseRostering::TabuSolver::init()
{
    srand( problem.randSeed );

    initAssistData();

    if (sln.genInitAssign() == false) {
        sln.repair();
    }

    sln.evaluateObjValue();

    optima = NurseRostering::Solver::Output( sln );
}

void NurseRostering::TabuSolver::solve()
{
    while (true) {
        // check time
        ++iterCount;
        if (isIterForTimeCheck( iterCount )) {
            if (clock() >= endTime) {
                break;
            }
        }

        // TODO

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
    : solver( s ), assign( s.problem.scenario.nurseNum, Weekday::SIZE ),
    missingNurseNums( s.problem.weekData.optNurseNums ),
    consecutives( s.problem.scenario.nurseNum ),
    totalAssignNums( s.problem.history.totalAssignNums )
{
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        consecutives[nurse] = Consecutive( solver.problem.history, nurse );
        assign[nurse][Weekday::HIS] = SingleAssign( s.problem.history.lastShifts[nurse] );
    }
}

bool NurseRostering::TabuSolver::Solution::genInitAssign()
{
    AvailableNurses availableNurse( *this );

    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
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
                        assignShift( weekday, nurse, shift, skill );
                    } else {
#ifdef INRC2_DEBUG
                        cerr << "fail to generate feasible solution." << endl;
#endif
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
    totalAssignNums = solver.problem.history.totalAssignNums;
    missingNurseNums = solver.problem.weekData.optNurseNums;
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        consecutives[nurse] = Consecutive( solver.problem.history, nurse );
    }
    assign = Assign( solver.problem.scenario.nurseNum, Weekday::SIZE );
}

void NurseRostering::TabuSolver::Solution::evaluateObjValue()
{
    objInsufficientStaff = 0;
    objConsecutiveShift = 0;
    objConsecutiveDay = 0;
    objConsecutiveDayOff = 0;
    objPreference = 0;
    objCompleteWeekend = 0;
    objTotalAssign = 0;
    objTotalWorkingWeekend = 0;

    evaluateInsufficientStaff();
    evaluateConsecutiveShift();
    evaluateConsecutiveDay();
    evaluateConsecutiveDayOff();
    evaluatePreference();
    evaluateCompleteWeekend();
    evaluateTotalAssign();
    evaluateTotalWorkingWeekend();

    objValue = objInsufficientStaff + objConsecutiveShift + objConsecutiveDay + objConsecutiveDayOff
        + objPreference + objCompleteWeekend + objTotalAssign + objTotalWorkingWeekend;
}

void NurseRostering::TabuSolver::Solution::repair()
{
    // TODO
    while (true) {
        bool feasible = genInitAssign();
        if (feasible) {
            break;
        } else {
            resetAssign();
        }
    }
}

void NurseRostering::TabuSolver::Solution::searchNeighborhood()
{
    // TODO

}

bool NurseRostering::TabuSolver::Solution::isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const
{
    return (!assign.isWorking( nurse, weekday - 1 )
        || solver.problem.scenario.shifts[assign[nurse][weekday - 1].shift].legalNextShifts[shift]);
}

NurseRostering::ObjValue NurseRostering::TabuSolver::Solution::tryAssignShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill )
{
    ShiftID originShift = assign[nurse][weekday].shift;
    SkillID originSkill = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if ((shift == NurseRostering::Scenario::Shift::ID_NONE) || (shift == originShift)
        || (originShift != NurseRostering::Scenario::Shift::ID_NONE)) {
        return 0;
    }

    if (!isValidSuccession( nurse, shift, weekday )) {
        return NurseRostering::MAX_OBJ_VALUE;
    }

    ObjValue delta = 0;
    ContractID contractID = solver.problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;
    int currentWeek = solver.problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    delta -= Penalty::InsufficientStaff *
        (missingNurseNums[weekday][shift][skill] > 0);

    // shift
    // DAY OFF AND DIFFERENT SHIFT IS NOT THE SAME CONDITION !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (weekday == Weekday::Sun) {  // there is no blocks on the right
        if (Weekday::Sun == c.dayLow[weekday]) {    // dayHigh[weekday] will always equal to Weekday::Sun
            delta -= Penalty::ConsecutiveDay *
                distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= Penalty::ConsecutiveDayOff *
                penaltyDayNum( 1, Weekday::Sun,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay * penaltyDayNum(
                Weekday::Sun - c.dayLow[Weekday::Sat], Weekday::Sun,
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        } else {    // block length over 1
            int consecutiveDayOfThisBlock = Weekday::Sun - c.dayLow[Weekday::Sun] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            }
        }
    } else {
        if (c.dayHigh[weekday] == c.dayLow[weekday]) {
            delta -= Penalty::ConsecutiveDay *
                distanceToRange( weekday - c.dayLow[weekday - 1],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= Penalty::ConsecutiveDayOff *
                distanceToRange( 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= Penalty::ConsecutiveDay * penaltyDayNum(
                c.dayHigh[weekday + 1] - weekday, c.dayHigh[weekday + 1],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDay * penaltyDayNum(
                c.dayHigh[weekday + 1] - c.dayLow[weekday - 1] + 1, c.dayHigh[weekday + 1],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        } else if (weekday == c.dayHigh[weekday]) {
            int consecutiveDayOfNextBlock = c.dayHigh[weekday + 1] - weekday;
            if (consecutiveDayOfNextBlock > contract.maxConsecutiveDayNum) {
                delta += Penalty::ConsecutiveDay;
            } else if ((c.dayHigh[weekday + 1] < Weekday::Sun)
                && (consecutiveDayOfNextBlock <= contract.minConsecutiveDayNum)) {
                delta -= Penalty::ConsecutiveDay;
            }
            int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            }
        } else if (weekday == c.dayLow[weekday]) {
            int consecutiveDayOfPrevBlock = weekday - c.dayLow[weekday - 1];
            if (consecutiveDayOfPrevBlock > contract.maxConsecutiveDayNum) {
                delta += Penalty::ConsecutiveDay;
            } else if (consecutiveDayOfPrevBlock <= contract.minConsecutiveDayNum) {
                delta -= Penalty::ConsecutiveDay;
            }
            int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            }
        } else {
            delta -= Penalty::ConsecutiveDayOff *
                distanceToRange( c.dayHigh[weekday] - c.dayLow[weekday] + 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDayOff *
                distanceToRange( weekday - c.dayLow[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay * distanceToRange( 1,
                contract.minConsecutiveDayNum, contract.minConsecutiveDayNum );
            delta += Penalty::ConsecutiveDayOff *
                penaltyDayNum( c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
        }
    }

    // day and day-off
    if (weekday == Weekday::Sun) {  // there is no blocks on the right
        if (Weekday::Sun == c.dayLow[weekday]) {    // dayHigh[weekday] will always equal to Weekday::Sun
            delta -= Penalty::ConsecutiveDay *
                distanceToRange( Weekday::Sun - c.dayLow[Weekday::Sat],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= Penalty::ConsecutiveDayOff *
                penaltyDayNum( 1, Weekday::Sun,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay * penaltyDayNum(
                Weekday::Sun - c.dayLow[Weekday::Sat], Weekday::Sun,
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        } else {    // block length over 1
            int consecutiveDayOfThisBlock = Weekday::Sun - c.dayLow[Weekday::Sun] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            }
        }
    } else {
        if (c.dayHigh[weekday] == c.dayLow[weekday]) {
            delta -= Penalty::ConsecutiveDay *
                distanceToRange( weekday - c.dayLow[weekday - 1],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta -= Penalty::ConsecutiveDayOff *
                distanceToRange( 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta -= Penalty::ConsecutiveDay * penaltyDayNum(
                c.dayHigh[weekday + 1] - weekday, c.dayHigh[weekday + 1],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
            delta += Penalty::ConsecutiveDay * penaltyDayNum(
                c.dayHigh[weekday + 1] - c.dayLow[weekday - 1] + 1, c.dayHigh[weekday + 1],
                contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
        } else if (weekday == c.dayHigh[weekday]) {
            int consecutiveDayOfNextBlock = c.dayHigh[weekday + 1] - weekday;
            if (consecutiveDayOfNextBlock > contract.maxConsecutiveDayNum) {
                delta += Penalty::ConsecutiveDay;
            } else if ((c.dayHigh[weekday + 1] < Weekday::Sun)
                && (consecutiveDayOfNextBlock <= contract.minConsecutiveDayNum)) {
                delta -= Penalty::ConsecutiveDay;
            }
            int consecutiveDayOfThisBlock = weekday - c.dayLow[weekday] + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            }
        } else if (weekday == c.dayLow[weekday]) {
            int consecutiveDayOfPrevBlock = weekday - c.dayLow[weekday - 1];
            if (consecutiveDayOfPrevBlock > contract.maxConsecutiveDayNum) {
                delta += Penalty::ConsecutiveDay;
            } else if (consecutiveDayOfPrevBlock <= contract.minConsecutiveDayNum) {
                delta -= Penalty::ConsecutiveDay;
            }
            int consecutiveDayOfThisBlock = c.dayHigh[weekday] - weekday + 1;
            if (consecutiveDayOfThisBlock > contract.maxConsecutiveDayoffNum) {
                delta -= Penalty::ConsecutiveDayOff;
            } else if (consecutiveDayOfThisBlock <= contract.minConsecutiveDayoffNum) {
                delta += Penalty::ConsecutiveDayOff;
            }
        } else {
            delta -= Penalty::ConsecutiveDayOff *
                distanceToRange( c.dayHigh[weekday] - c.dayLow[weekday] + 1,
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDayOff *
                distanceToRange( weekday - c.dayLow[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            delta += Penalty::ConsecutiveDay * distanceToRange( 1,
                contract.minConsecutiveDayNum, contract.minConsecutiveDayNum );
            delta += Penalty::ConsecutiveDayOff *
                penaltyDayNum( c.dayHigh[weekday] - weekday, c.dayHigh[weekday],
                contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
        }
    }

    const WeekData &weekData( solver.problem.weekData );
    delta += Penalty::Preference *
        weekData.shiftOffs[weekday][shift][skill];

    if (contract.completeWeekend) {
        int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
        if (assign.isWorking( nurse, theOtherDay )) {
            delta -= Penalty::CompleteWeekend;
        } else {
            delta += Penalty::CompleteWeekend;
        }
    }

    if ((totalAssignNums[nurse] * totalWeekNum) < (contract.minShiftNum * currentWeek)) {
        delta -= Penalty::TotalAssign;
    } else if ((totalAssignNums[nurse] * totalWeekNum) > (contract.maxShiftNum * currentWeek)) {
        delta += Penalty::TotalAssign;
    }

    delta += Penalty::TotalWorkingWeekend * ((weekday > Weekday::Fri)
        && !assign.isWorking( nurse, Weekday::Sat ) && !assign.isWorking( nurse, Weekday::Sun ));

    return delta;
}

NurseRostering::ObjValue NurseRostering::TabuSolver::Solution::tryChangeShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill )
{
    ShiftID originShift = assign[nurse][weekday].shift;
    SkillID originSkill = assign[nurse][weekday].skill;
    // TODO : make sure they won't be the same and leave out this
    if ((shift == NurseRostering::Scenario::Shift::ID_NONE) || (shift == originShift)) {
        return 0;
    }

    const WeekData &weekData( solver.problem.weekData );
    if (((weekData.optNurseNums[weekday][originShift][originSkill] -
        missingNurseNums[weekday][originShift][originSkill])
        < weekData.minNurseNums[weekday][originShift][originSkill])
        || !isValidSuccession( nurse, shift, weekday )) {
        return NurseRostering::MAX_OBJ_VALUE;
    }

    ObjValue delta = 0;
    ContractID contractID = solver.problem.scenario.nurses[nurse].contract;
    const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;
    int currentWeek = solver.problem.history.currentWeek;
    const Consecutive &c( consecutives[nurse] );

    delta += Penalty::InsufficientStaff *
        (missingNurseNums[weekday][originShift][originSkill] >= 0);
    delta -= Penalty::InsufficientStaff *
        (missingNurseNums[weekday][shift][skill] > 0);



    if (!assign.isWorking( nurse, weekday )) {
        if (c.dayLow[weekday] == Weekday::Mon) {

        } else if (c.dayHigh[weekday] == Weekday::Sun) {

        } else {
            if ((weekday != c.dayHigh[weekday]) && (weekday != c.dayLow[weekday])) {
                delta -= Penalty::ConsecutiveDayOff *
                    distanceToRange( c.dayHigh[weekday] - c.dayLow[weekday] + 1,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta += Penalty::ConsecutiveDayOff *
                    distanceToRange( weekday - c.dayLow[weekday],
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                delta += Penalty::ConsecutiveDay * distanceToRange( 1,
                    contract.minConsecutiveDayNum, contract.minConsecutiveDayNum );
                delta += Penalty::ConsecutiveDayOff *
                    distanceToRange( c.dayHigh[weekday] - weekday,
                    contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
            } else {

            }
        }
    }

    delta -= Penalty::Preference *
        weekData.shiftOffs[weekday][originShift][originSkill];
    delta += Penalty::Preference *
        weekData.shiftOffs[weekday][shift][skill];

    if (contract.completeWeekend) {
        int theOtherDay = Weekday::Sat + (weekday == Weekday::Sat);
        if (assign.isWorking( nurse, theOtherDay )) {
            delta -= Penalty::CompleteWeekend;
        } else {
            delta += Penalty::CompleteWeekend;
        }
    }

    if (!assign.isWorking( nurse, weekday )) {
        if ((totalAssignNums[nurse] * totalWeekNum) < (contract.minShiftNum * currentWeek)) {
            delta -= Penalty::TotalAssign;
        } else if ((totalAssignNums[nurse] * totalWeekNum) > (contract.maxShiftNum * currentWeek)) {
            delta += Penalty::TotalAssign;
        }
    }

    delta += Penalty::TotalWorkingWeekend * ((weekday > Weekday::Fri)
        && !assign.isWorking( nurse, Weekday::Sat ) && !assign.isWorking( nurse, Weekday::Sun ));

    return delta;
}

NurseRostering::ObjValue NurseRostering::TabuSolver::Solution::tryRemoveShift( int weekday, NurseID nurse )
{
    // TODO : make sure they won't be the same and leave out this
    if (!assign.isWorking( nurse, weekday )) {
        return 0;
    }

    ObjValue delta = 0;



    return delta;
}

void NurseRostering::TabuSolver::Solution::assignShift( int weekday, NurseID nurse, ShiftID shift, SkillID skill )
{
    // TODO : make sure they won't be the same and leave out this
    if ((shift == NurseRostering::Scenario::Shift::ID_NONE)
        || (shift == assign[nurse][weekday].shift)) {
        return;
    }

    updateConsecutive( weekday, nurse, shift );

    --missingNurseNums[weekday][shift][skill];
    if (assign.isWorking( nurse, weekday )) {
        ++missingNurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];
    }

    if (!assign.isWorking( nurse, weekday )) {
        ++totalAssignNums[nurse];
    }

    assign[nurse][weekday] = SingleAssign( shift, skill );
}

void NurseRostering::TabuSolver::Solution::removeShift( int weekday, NurseID nurse )
{
    // TODO : make sure they won't be the same and leave out this
    if (!assign.isWorking( nurse, weekday )) {
        return;
    }

    updateConsecutive( weekday, nurse, NurseRostering::Scenario::Shift::ID_NONE );

    ++missingNurseNums[weekday][assign[nurse][weekday].shift][assign[nurse][weekday].skill];

    --totalAssignNums[nurse];

    assign[nurse][weekday] = SingleAssign();
}

void NurseRostering::TabuSolver::Solution::updateConsecutive( int weekday, NurseID nurse, ShiftID shift )
{
    Consecutive &c( consecutives[nurse] );
    int nextDay = weekday + 1;
    int prevDay = weekday - 1;

    if ((weekday != c.dayHigh[weekday]) && (weekday != c.dayLow[weekday])) {
        if (assign.isWorking( nurse, weekday ) != Assign::isWorking( shift )) {
            assignMiddle( weekday, nextDay, prevDay, c.dayHigh, c.dayLow );
        }
        // consider shift
        if ((weekday != c.shiftHigh[weekday]) && (weekday != c.shiftLow[weekday])) {
            assignMiddle( weekday, nextDay, prevDay, c.shiftHigh, c.shiftLow );
        } else {
            bool isShiftHigh = (weekday == c.shiftHigh[weekday]);
            bool isShiftLow = (weekday == c.shiftLow[weekday]);
            if (isShiftHigh) {
                assignHigh( weekday, nextDay, prevDay, c.shiftHigh, c.shiftLow,
                    ((weekday != Weekday::Sun) && (shift == assign[nurse][nextDay].shift)) );
            }
            if (isShiftLow) {
                assignLow( weekday, nextDay, prevDay, c.shiftHigh, c.shiftLow,
                    (shift == assign[nurse][prevDay].shift) );
            }
        }
    } else {    // on the border of consecutive days
        bool isDayHigh = (weekday == c.dayHigh[weekday]);
        bool isDayLow = (weekday == c.dayLow[weekday]);
        if (isDayHigh) {
            if (assign.isWorking( nurse, weekday ) != Assign::isWorking( shift )) {
                assignHigh( weekday, nextDay, prevDay, c.dayHigh, c.dayLow,
                    (weekday != Weekday::Sun) );
            }
            // argument affectRight will guarantee that there is no shift on the right 
            assignHigh( weekday, nextDay, prevDay, c.shiftHigh, c.shiftLow,
                ((weekday != Weekday::Sun) && (shift == assign[nurse][nextDay].shift)) );
        }
        if (isDayLow) {
            if (assign.isWorking( nurse, weekday ) != Assign::isWorking( shift )) {
                assignLow( weekday, nextDay, prevDay, c.dayHigh, c.dayLow, true );
            }
            // argument affectLeft will guarantee that there is no shift on the left 
            assignLow( weekday, nextDay, prevDay, c.shiftHigh, c.shiftLow,
                (shift == assign[nurse][prevDay].shift) );
        }
    }
}


void NurseRostering::TabuSolver::Solution::assignHigh( int weekday, int nextDay, int prevDay, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectRight )
{
    for (int d = prevDay; (d >= Weekday::HIS) && (d >= low[weekday]); --d) {
        high[d] = prevDay;
    }
    if (affectRight) {
        for (int d = nextDay; d <= high[nextDay]; ++d) {
            low[d] = weekday;
        }
        high[weekday] = high[nextDay];
    } else {
        high[weekday] = weekday;
    }
    low[weekday] = weekday;
}

void NurseRostering::TabuSolver::Solution::assignLow( int weekday, int nextDay, int prevDay, int high[Weekday::SIZE], int low[Weekday::SIZE], bool affectLeft )
{
    for (int d = nextDay; d <= high[weekday]; ++d) {
        low[d] = nextDay;
    }
    if (affectLeft) {
        for (int d = prevDay; (d >= Weekday::HIS) && (d >= low[prevDay]); --d) {
            high[d] = weekday;
        }
        low[weekday] = low[prevDay];
    } else {
        low[weekday] = weekday;
    }
    high[weekday] = weekday;
}

void NurseRostering::TabuSolver::Solution::assignMiddle( int weekday, int nextDay, int prevDay, int high[Weekday::SIZE], int low[Weekday::SIZE] )
{
    for (int d = nextDay; d <= high[weekday]; ++d) {
        low[d] = nextDay;
    }
    for (int d = prevDay; (d >= Weekday::HIS) && (d >= low[weekday]); --d) {
        high[d] = prevDay;
    }
    high[weekday] = weekday;
    low[weekday] = weekday;
}

void NurseRostering::TabuSolver::Solution::evaluateInsufficientStaff()
{
    for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
        for (ShiftID shift = 0; shift < solver.problem.scenario.shiftTypeNum; ++shift) {
            for (SkillID skill = 0; skill < solver.problem.scenario.skillTypeNum; ++skill) {
                if (missingNurseNums[weekday][shift][skill] > 0) {
                    objInsufficientStaff += Penalty::InsufficientStaff * missingNurseNums[weekday][shift][skill];
                }
            }
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateConsecutiveShift()
{
    const History &history( solver.problem.history );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const vector<Scenario::Shift> &shifts( solver.problem.scenario.shifts );

        int nextday = c.shiftHigh[Weekday::Mon] + 1;
        if (nextday < Weekday::SIZE) {   // the entire week is not one block
            // handle first block with history
            if (assign.isWorking( nurse, Weekday::Mon )) {
                const Scenario::Shift &shift( shifts[assign[nurse][Weekday::Mon].shift] );
                if (history.lastShifts[nurse] == assign[nurse][Weekday::Mon].shift) {
                    if (history.consecutiveShiftNums[nurse] > shift.maxConsecutiveShiftNum) {
                        // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                        objConsecutiveShift += Penalty::ConsecutiveShift * (c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1);
                    } else {
                        objConsecutiveShift += Penalty::ConsecutiveShift * distanceToRange(
                            c.shiftHigh[Weekday::Mon] - c.shiftLow[Weekday::Mon] + 1,
                            shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    }
                } else {
                    objConsecutiveShift += Penalty::ConsecutiveShift * distanceToRange(
                        c.shiftHigh[Weekday::Mon] - Weekday::Mon + 1, shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                    if (Assign::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else if (Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                    history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
            }
            // handle blocks in the middle of the week
            for (; c.shiftHigh[nextday] < Weekday::Sun; nextday = c.shiftHigh[nextday] + 1) {
                if (assign.isWorking( nurse, nextday )) {
                    const ShiftID &shiftID( assign[nurse][nextday].shift );
                    const Scenario::Shift &shift( shifts[shiftID] );
                    objConsecutiveShift += Penalty::ConsecutiveShift *
                        distanceToRange( c.shiftHigh[nextday] - c.shiftLow[nextday] + 1,
                        shift.minConsecutiveShiftNum, shift.maxConsecutiveShiftNum );
                }
            }
        }
        // handle last consecutive block
        int consecutiveShift_EntireWeek = history.consecutiveShiftNums[nurse] + Weekday::NUM;
        int consecutiveShift = c.shiftHigh[Weekday::Sun] - c.shiftLow[Weekday::Sun] + 1;
        if (assign.isWorking( nurse, Weekday::Sun )) {
            const ShiftID &shiftID( assign[nurse][Weekday::Sun].shift );
            const Scenario::Shift &shift( solver.problem.scenario.shifts[shiftID] );
            if (c.isSingleConsecutiveShift()) { // the entire week is one block
                if (history.lastShifts[nurse] == assign[nurse][Weekday::Sun].shift) {
                    if (history.consecutiveShiftNums[nurse] > shift.maxConsecutiveShiftNum) {
                        objConsecutiveShift += Penalty::ConsecutiveShift * Weekday::NUM;
                    } else {
                        objConsecutiveShift += Penalty::ConsecutiveShift * exceedCount(
                            consecutiveShift_EntireWeek, shift.maxConsecutiveShiftNum );
                    }
                } else {    // different shifts
                    if (Weekday::NUM > shift.maxConsecutiveShiftNum) {
                        objConsecutiveShift += Penalty::ConsecutiveShift *
                            (shift.maxConsecutiveShiftNum - Weekday::NUM);
                    }
                    if (Assign::isWorking( history.lastShifts[nurse] )) {
                        objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                            history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
                    }
                }
            } else {
                objConsecutiveShift += Penalty::ConsecutiveShift * exceedCount(
                    consecutiveShift, shift.maxConsecutiveShiftNum );
            }
        } else if (c.isSingleConsecutiveShift() // the entire week is one block
            && Assign::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveShift += Penalty::ConsecutiveShift * absentCount(
                history.consecutiveShiftNums[nurse], shifts[history.lastShifts[nurse]].minConsecutiveShiftNum );
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateConsecutiveDay()
{
    const History &history( solver.problem.history );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const ContractID &contractID( solver.problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );

        int nextday = c.dayHigh[Weekday::Mon] + 1;
        if (nextday < Weekday::SIZE) {   // the entire week is not one block
            // handle first block with history
            if (assign.isWorking( nurse, Weekday::Mon )) {
                if (history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                    // (high - low + 1) which low is Mon for exceeding part in previous week has been counted
                    objConsecutiveDay += Penalty::ConsecutiveDay * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDay += Penalty::ConsecutiveDay * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
            } else if (Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveDay += Penalty::ConsecutiveDay * absentCount
                    ( history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
            }
            // handle blocks in the middle of the week
            for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
                if (assign.isWorking( nurse, nextday )) {
                    objConsecutiveDay += Penalty::ConsecutiveDay *
                        distanceToRange( c.dayHigh[nextday] - c.dayLow[nextday] + 1,
                        contract.minConsecutiveDayNum, contract.maxConsecutiveDayNum );
                }
            }
        }
        // handle last consecutive block
        int consecutiveDay = c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
        if (assign.isWorking( nurse, Weekday::Sun )) {
            if (c.isSingleConsecutiveDay()) {   // the entire week is one block
                if (history.consecutiveDayNums[nurse] > contract.maxConsecutiveDayNum) {
                    objConsecutiveDay += Penalty::ConsecutiveDay * Weekday::NUM;
                } else {
                    objConsecutiveDay += Penalty::ConsecutiveDay * exceedCount(
                        consecutiveDay, contract.maxConsecutiveDayNum );
                }
            } else {
                objConsecutiveDay += Penalty::ConsecutiveDay * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayNum );
            }
        } else if (c.isSingleConsecutiveDay() // the entire week is one block
            && Assign::isWorking( history.lastShifts[nurse] )) {
            objConsecutiveDay += Penalty::ConsecutiveDay * absentCount(
                history.consecutiveDayNums[nurse], contract.minConsecutiveDayNum );
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateConsecutiveDayOff()
{
    const History &history( solver.problem.history );
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        const Consecutive &c( consecutives[nurse] );
        const ContractID &contractID( solver.problem.scenario.nurses[nurse].contract );
        const Scenario::Contract &contract( solver.problem.scenario.contracts[contractID] );

        int nextday = c.dayHigh[Weekday::Mon] + 1;
        if (nextday < Weekday::SIZE) {   // the entire week is not one block
            // handle first block with history
            if (!assign.isWorking( nurse, Weekday::Mon )) {
                if (history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * (c.dayHigh[Weekday::Mon] - Weekday::Mon + 1);
                } else {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * distanceToRange(
                        c.dayHigh[Weekday::Mon] - c.dayLow[Weekday::Mon] + 1,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
            } else if (!Assign::isWorking( history.lastShifts[nurse] )) {
                objConsecutiveDayOff += Penalty::ConsecutiveDayOff * absentCount(
                    history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
            }
            // handle blocks in the middle of the week
            for (; c.dayHigh[nextday] < Weekday::Sun; nextday = c.dayHigh[nextday] + 1) {
                if (!assign.isWorking( nurse, nextday )) {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff *
                        distanceToRange( c.dayHigh[nextday] - c.dayLow[nextday] + 1,
                        contract.minConsecutiveDayoffNum, contract.maxConsecutiveDayoffNum );
                }
            }
        }
        // handle last consecutive block
        int consecutiveDay = c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
        if (!assign.isWorking( nurse, Weekday::Sun )) {
            if (c.isSingleConsecutiveDay()) {   // the entire week is one block
                if (history.consecutiveDayoffNums[nurse] > contract.maxConsecutiveDayoffNum) {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * Weekday::NUM;
                } else {
                    objConsecutiveDayOff += Penalty::ConsecutiveDayOff * exceedCount(
                        consecutiveDay, contract.maxConsecutiveDayoffNum );
                }
            } else {
                objConsecutiveDayOff += Penalty::ConsecutiveDayOff * exceedCount(
                    consecutiveDay, contract.maxConsecutiveDayoffNum );
            }
        } else if (c.isSingleConsecutiveDay() // the entire week is one block
            && (!Assign::isWorking( history.lastShifts[nurse] ))) {
            objConsecutiveDayOff += Penalty::ConsecutiveDayOff * absentCount(
                history.consecutiveDayoffNums[nurse], contract.minConsecutiveDayoffNum );
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluatePreference()
{
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        for (int weekday = Weekday::Mon; weekday < Weekday::SIZE; ++weekday) {
            const ShiftID &shift = assign[nurse][weekday].shift;
            if (shift != NurseRostering::Scenario::Shift::ID_NONE) {
                objPreference += Penalty::Preference *
                    solver.problem.weekData.shiftOffs[weekday][shift][nurse];
            }
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateCompleteWeekend()
{
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        objCompleteWeekend += Penalty::CompleteWeekend *
            (solver.problem.scenario.contracts[solver.problem.scenario.nurses[nurse].contract].completeWeekend
            && (assign.isWorking( nurse, Weekday::Sat )
            != assign.isWorking( nurse, Weekday::Sun )));
    }
}

void NurseRostering::TabuSolver::Solution::evaluateTotalAssign()
{
    const Scenario &scenario( solver.problem.scenario );
    int totalWeekNum = solver.problem.scenario.totalWeekNum;

    for (NurseID nurse = 0; nurse < scenario.nurseNum; ++nurse) {
        int min = scenario.contracts[scenario.nurses[nurse].contract].minShiftNum;
        int max = scenario.contracts[scenario.nurses[nurse].contract].maxShiftNum;
        objTotalAssign += Penalty::TotalAssign * distanceToRange(
            totalAssignNums[nurse] * totalWeekNum,
            min * solver.problem.history.currentWeek,
            max * solver.problem.history.currentWeek ) / totalWeekNum;
        // remove penalty in the history except the first week
        if (solver.problem.history.pastWeekCount > 0) {
            objTotalAssign -= Penalty::TotalAssign * distanceToRange(
                solver.problem.history.totalAssignNums[nurse] * totalWeekNum,
                min * solver.problem.history.pastWeekCount,
                max * solver.problem.history.pastWeekCount ) / totalWeekNum;
        }
    }
}

void NurseRostering::TabuSolver::Solution::evaluateTotalWorkingWeekend()
{
    int totalWeekNum = solver.problem.scenario.totalWeekNum;

    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        int maxWeekend = solver.problem.scenario.contracts[solver.problem.scenario.nurses[nurse].contract].maxWorkingWeekendNum;
        int historyWeekend = solver.problem.history.totalWorkingWeekendNums[nurse] * totalWeekNum;
        int exceedingWeekend = historyWeekend - (maxWeekend * solver.problem.history.currentWeek) +
            ((assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun )) * totalWeekNum);
        if (exceedingWeekend > 0) {
            objTotalWorkingWeekend += Penalty::TotalWorkingWeekend *
                exceedingWeekend / totalWeekNum;
        }
        // remove penalty in the history except the first week
        if (solver.problem.history.pastWeekCount > 0) {
            historyWeekend -= maxWeekend * solver.problem.history.pastWeekCount;
            if (historyWeekend > 0) {
                objTotalWorkingWeekend -= Penalty::TotalWorkingWeekend *
                    historyWeekend / totalWeekNum;
            }
        }
    }
}

NurseRostering::History NurseRostering::TabuSolver::Solution::genHistory() const
{
    const History &history( solver.problem.history );
    History newHistory;
    newHistory.lastShifts.resize( solver.problem.scenario.nurseNum );
    newHistory.consecutiveShiftNums.resize( solver.problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayNums.resize( solver.problem.scenario.nurseNum, 0 );
    newHistory.consecutiveDayoffNums.resize( solver.problem.scenario.nurseNum, 0 );

    newHistory.pastWeekCount = history.currentWeek;
    newHistory.currentWeek = history.currentWeek + 1;
    newHistory.totalAssignNums = totalAssignNums;
    newHistory.totalWorkingWeekendNums = history.totalWorkingWeekendNums;
    for (NurseID nurse = 0; nurse < solver.problem.scenario.nurseNum; ++nurse) {
        newHistory.totalWorkingWeekendNums[nurse] +=
            (assign.isWorking( nurse, Weekday::Sat ) || assign.isWorking( nurse, Weekday::Sun ));
        newHistory.lastShifts[nurse] = assign[nurse][Weekday::Sun].shift;
        const Consecutive &c( consecutives[nurse] );
        if (assign.isWorking( nurse, Weekday::Sun )) {
            newHistory.consecutiveShiftNums[nurse] =
                c.shiftHigh[Weekday::Sun] - c.shiftLow[Weekday::Sun] + 1;
            if (c.isSingleConsecutiveShift()
                && (history.lastShifts[nurse] == assign[nurse][Weekday::Sun].shift)) {
                newHistory.consecutiveShiftNums[nurse] += history.consecutiveShiftNums[nurse];
            }
            newHistory.consecutiveDayNums[nurse] =
                c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
            if (c.isSingleConsecutiveDay()) {
                newHistory.consecutiveDayNums[nurse] += history.consecutiveDayNums[nurse];
            }
        } else {
            newHistory.consecutiveDayoffNums[nurse] =
                c.dayHigh[Weekday::Sun] - c.dayLow[Weekday::Sun] + 1;
            if (c.isSingleConsecutiveDay()) {
                newHistory.consecutiveDayoffNums[nurse] += history.consecutiveDayoffNums[nurse];
            }
        }
    }

    return newHistory;
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
            if (sln.getAssign().isWorking( nurse, weekday )) { // set the nurse invalid for current day
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
