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


void NurseRostering::Penalty::setDefaultMode()
{
    singleAssign = DefaultPenalty::FORBIDDEN_MOVE;
    underStaff = DefaultPenalty::FORBIDDEN_MOVE;
    succession = DefaultPenalty::FORBIDDEN_MOVE;
    missSkill = DefaultPenalty::FORBIDDEN_MOVE;

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
    singleAssign = DefaultPenalty::FORBIDDEN_MOVE;
    underStaff = 0;
    succession = DefaultPenalty::FORBIDDEN_MOVE;
    missSkill = DefaultPenalty::FORBIDDEN_MOVE;

    insufficientStaff = 0;
    consecutiveShift = DefaultPenalty::ConsecutiveShift;
    consecutiveDay = DefaultPenalty::ConsecutiveDay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    preference = DefaultPenalty::Preference;
    completeWeekend = DefaultPenalty::CompleteWeekend;
    totalAssign = DefaultPenalty::TotalAssign;
    totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::setRepairMode( ObjValue WeightOnUnderStaff, ObjValue WeightOnSuccesion, ObjValue softConstraintDecay )
{
    singleAssign = DefaultPenalty::FORBIDDEN_MOVE;
    underStaff = WeightOnUnderStaff;
    succession = WeightOnSuccesion;
    missSkill = DefaultPenalty::FORBIDDEN_MOVE;

    insufficientStaff = DefaultPenalty::InsufficientStaff / softConstraintDecay;
    consecutiveShift = DefaultPenalty::ConsecutiveShift / softConstraintDecay;
    consecutiveDay = DefaultPenalty::ConsecutiveDay / softConstraintDecay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff / softConstraintDecay;
    preference = DefaultPenalty::Preference / softConstraintDecay;
    completeWeekend = DefaultPenalty::CompleteWeekend / softConstraintDecay;
    totalAssign = DefaultPenalty::TotalAssign / softConstraintDecay;
    totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend / softConstraintDecay;
}
