#include "NurseRostering.h"


using namespace std;


const Timer::Duration NurseRostering::MIN_RUNNING_TIME = Timer::Duration( 0 );
const NurseRostering::IterCount NurseRostering::MAX_ITER_COUNT = (1 << 30);


const NurseRostering::NurseID NurseRostering::Scenario::Nurse::ID_NONE = -1;

const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_ANY = -1;
const std::string NurseRostering::Scenario::Shift::NAME_ANY( "Any" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_NONE = 0;
const std::string NurseRostering::Scenario::Shift::NAME_NONE( "None" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_BEGIN = NurseRostering::Scenario::Shift::ID_NONE + 1;

const NurseRostering::SkillID NurseRostering::Scenario::Skill::ID_BEGIN = 0;



NurseRostering::NurseRostering()
{
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_ANY] = NurseRostering::Scenario::Shift::ID_ANY;
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_NONE] = NurseRostering::Scenario::Shift::ID_NONE;
}

void NurseRostering::adjustRangeOfTotalAssignByWorkload()
{
    for (NurseID nurse = 0; nurse < scenario.nurseNum; ++nurse) {
        const Scenario::Contract &c( scenario.contracts[scenario.nurses[nurse].contract] );
#ifdef INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
        int weekToStartCountMin = scenario.totalWeekNum * c.minShiftNum;
        bool ignoreMinShift = ((history.currentWeek * c.maxShiftNum) < weekToStartCountMin);
        scenario.nurses[nurse].restMinShiftNum = (ignoreMinShift) ? 0 : (c.minShiftNum - history.totalAssignNums[nurse]);
#else
        scenario.nurses[nurse].restMinShiftNum = c.minShiftNum - history.totalAssignNums[nurse];
#endif
        scenario.nurses[nurse].restMaxShiftNum = c.maxShiftNum - history.totalAssignNums[nurse];
        scenario.nurses[nurse].restMaxWorkingWeekendNum = c.maxWorkingWeekendNum - history.totalWorkingWeekendNums[nurse];
    }
}


void NurseRostering::Penalty::reset()
{
    modeStack.clear();

    pm.singleAssign = DefaultPenalty::FORBIDDEN_MOVE;
    pm.underStaff = DefaultPenalty::FORBIDDEN_MOVE;
    pm.succession = DefaultPenalty::FORBIDDEN_MOVE;
    pm.missSkill = DefaultPenalty::FORBIDDEN_MOVE;

    pm.insufficientStaff = DefaultPenalty::InsufficientStaff;
    pm.consecutiveShift = DefaultPenalty::ConsecutiveShift;
    pm.consecutiveDay = DefaultPenalty::ConsecutiveDay;
    pm.consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    pm.preference = DefaultPenalty::Preference;
    pm.completeWeekend = DefaultPenalty::CompleteWeekend;
    pm.totalAssign = DefaultPenalty::TotalAssign;
    pm.totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::recoverLastMode()
{
    pm = modeStack.back();
    modeStack.pop_back();
}

void NurseRostering::Penalty::setSwapMode()
{
    modeStack.push_back( pm );

    pm.underStaff = 0;          // due to no extra assignments
    pm.insufficientStaff = 0;   // due to no extra assignments
}

void NurseRostering::Penalty::setBlockSwapMode()
{
    modeStack.push_back( pm );

    pm.underStaff = 0;  // due to no extra assignments
    pm.succession = 0;  // due to it is checked manually
    pm.missSkill = 0;   // due to it is checked manually
    pm.insufficientStaff = 0;   // due to no extra assignments
}

void NurseRostering::Penalty::setExchangeMode()
{
    modeStack.push_back( pm );

    pm.succession = 0;  // due to it is checked manually
    pm.missSkill = 0;   // due to it is the same nurse
    pm.totalAssign = 0; // due to it is the same nurse
}

void NurseRostering::Penalty::setRepairMode( ObjValue WeightOnUnderStaff, ObjValue WeightOnSuccesion, ObjValue softConstraintDecay )
{
    modeStack.push_back( pm );

    pm.underStaff = WeightOnUnderStaff;
    pm.succession = WeightOnSuccesion;
    pm.insufficientStaff = DefaultPenalty::InsufficientStaff / softConstraintDecay;
    pm.consecutiveShift = DefaultPenalty::ConsecutiveShift / softConstraintDecay;
    pm.consecutiveDay = DefaultPenalty::ConsecutiveDay / softConstraintDecay;
    pm.consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff / softConstraintDecay;
    pm.preference = DefaultPenalty::Preference / softConstraintDecay;
    pm.completeWeekend = DefaultPenalty::CompleteWeekend / softConstraintDecay;
    pm.totalAssign = DefaultPenalty::TotalAssign / softConstraintDecay;
    pm.totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend / softConstraintDecay;
}
