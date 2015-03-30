#include "NurseRostering.h"


using namespace std;


const clock_t NurseRostering::MAX_RUNNING_TIME = (1 << 30);


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


void NurseRostering::Penalty::setNormalMode()
{
    singleAssign = DefaultPenalty::MAX_OBJ_VALUE;
    underStaff = DefaultPenalty::MAX_OBJ_VALUE;
    succession = DefaultPenalty::MAX_OBJ_VALUE;
    missSkill = DefaultPenalty::MAX_OBJ_VALUE;

    insufficientStaff = DefaultPenalty::InsufficientStaff;
    consecutiveShift = DefaultPenalty::ConsecutiveShift;
    consecutiveDay = DefaultPenalty::ConsecutiveDay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    preference = DefaultPenalty::Preference;
    completeWeekend = DefaultPenalty::CompleteWeekend;
    totalAssign = DefaultPenalty::TotalAssign;
    totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::setSwapMode()
{
    singleAssign = DefaultPenalty::MAX_OBJ_VALUE;
    underStaff = 0;
    succession = DefaultPenalty::MAX_OBJ_VALUE;
    missSkill = DefaultPenalty::MAX_OBJ_VALUE;

    insufficientStaff = 0;
    consecutiveShift = DefaultPenalty::ConsecutiveShift;
    consecutiveDay = DefaultPenalty::ConsecutiveDay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    preference = DefaultPenalty::Preference;
    completeWeekend = DefaultPenalty::CompleteWeekend;
    totalAssign = DefaultPenalty::TotalAssign;
    totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::setRepairMode( ObjValue softConstraintDecay, ObjValue WeightOnUnderStaff, ObjValue WeightOnSuccesion )
{
    singleAssign = DefaultPenalty::MAX_OBJ_VALUE;
    underStaff = WeightOnUnderStaff;
    succession = WeightOnSuccesion;
    missSkill = DefaultPenalty::MAX_OBJ_VALUE;

    insufficientStaff = DefaultPenalty::InsufficientStaff / softConstraintDecay;
    consecutiveShift = DefaultPenalty::ConsecutiveShift / softConstraintDecay;
    consecutiveDay = DefaultPenalty::ConsecutiveDay / softConstraintDecay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff / softConstraintDecay;
    preference = DefaultPenalty::Preference / softConstraintDecay;
    completeWeekend = DefaultPenalty::CompleteWeekend / softConstraintDecay;
    totalAssign = DefaultPenalty::TotalAssign / softConstraintDecay;
    totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend / softConstraintDecay;
}

