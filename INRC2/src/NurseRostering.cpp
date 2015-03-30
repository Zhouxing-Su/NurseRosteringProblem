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
    SingleAssign = DefaultPenalty::MAX_OBJ_VALUE;
    UnderStaff = DefaultPenalty::MAX_OBJ_VALUE;
    Succession = DefaultPenalty::MAX_OBJ_VALUE;
    MissSkill = DefaultPenalty::MAX_OBJ_VALUE;

    InsufficientStaff = DefaultPenalty::InsufficientStaff;
    ConsecutiveShift = DefaultPenalty::ConsecutiveShift;
    ConsecutiveDay = DefaultPenalty::ConsecutiveDay;
    ConsecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    Preference = DefaultPenalty::Preference;
    CompleteWeekend = DefaultPenalty::CompleteWeekend;
    TotalAssign = DefaultPenalty::TotalAssign;
    TotalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::setSwapMode()
{
    SingleAssign = DefaultPenalty::MAX_OBJ_VALUE;
    UnderStaff = 0;
    Succession = DefaultPenalty::MAX_OBJ_VALUE;
    MissSkill = DefaultPenalty::MAX_OBJ_VALUE;

    InsufficientStaff = 0;
    ConsecutiveShift = DefaultPenalty::ConsecutiveShift;
    ConsecutiveDay = DefaultPenalty::ConsecutiveDay;
    ConsecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    Preference = DefaultPenalty::Preference;
    CompleteWeekend = DefaultPenalty::CompleteWeekend;
    TotalAssign = DefaultPenalty::TotalAssign;
    TotalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::setRepairMode( ObjValue softConstraintDecay, ObjValue WeightOnUnderStaff, ObjValue WeightOnSuccesion )
{
    SingleAssign = DefaultPenalty::MAX_OBJ_VALUE;
    UnderStaff = WeightOnUnderStaff;
    Succession = WeightOnSuccesion;
    MissSkill = DefaultPenalty::MAX_OBJ_VALUE;

    InsufficientStaff = DefaultPenalty::InsufficientStaff / softConstraintDecay;
    ConsecutiveShift = DefaultPenalty::ConsecutiveShift / softConstraintDecay;
    ConsecutiveDay = DefaultPenalty::ConsecutiveDay / softConstraintDecay;
    ConsecutiveDayOff = DefaultPenalty::ConsecutiveDayOff / softConstraintDecay;
    Preference = DefaultPenalty::Preference / softConstraintDecay;
    CompleteWeekend = DefaultPenalty::CompleteWeekend / softConstraintDecay;
    TotalAssign = DefaultPenalty::TotalAssign / softConstraintDecay;
    TotalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend / softConstraintDecay;
}
