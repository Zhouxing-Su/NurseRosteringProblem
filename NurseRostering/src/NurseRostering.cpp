#include "NurseRostering.h"


using namespace std;


const clock_t NurseRostering::MAX_RUNNING_TIME = (1 << 30);


const NurseRostering::NurseID NurseRostering::Scenario::Nurse::ID_NONE = -1;

const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_ANY = -1;
const std::string NurseRostering::Scenario::Shift::NAME_ANY( "Any" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_NONE = 0;
const std::string NurseRostering::Scenario::Shift::NAME_NONE( "None" );
const NurseRostering::ShiftID NurseRostering::Scenario::Shift::ID_BEGIN = NurseRostering::Scenario::Shift::ID_NONE + 1;

const NurseRostering::SkillID NurseRostering::Scenario::Skill::ID_NONE = 0;
const NurseRostering::SkillID NurseRostering::Scenario::Skill::ID_BEGIN = NurseRostering::Scenario::Skill::ID_NONE + 1;



NurseRostering::NurseRostering()
{
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_ANY] = NurseRostering::Scenario::Shift::ID_ANY;
    names.shiftMap[NurseRostering::Scenario::Shift::NAME_NONE] = NurseRostering::Scenario::Shift::ID_NONE;
}

void NurseRostering::adjustRangeOfTotalAssignByWeekCount()
{
    if (scenario.totalWeekNum > 1) {  // it must be count if there is only one week
        for (auto iter = scenario.contracts.begin(); iter != scenario.contracts.end(); ++iter) {
            int deltaAmp = iter->minShiftNum / scenario.totalWeekNum;
            int delta_lastWeek = deltaAmp * (history.pastWeekCount - scenario.totalWeekNum - 1) / scenario.totalWeekNum;
            iter->minShiftNum_lastWeek = delta_lastWeek + iter->minShiftNum  * history.pastWeekCount / scenario.totalWeekNum;
            if (history.currentWeek < scenario.totalWeekNum) {
                int delta = deltaAmp * (history.pastWeekCount - scenario.totalWeekNum) / scenario.totalWeekNum;
                iter->minShiftNum = delta + iter->minShiftNum  * history.currentWeek / scenario.totalWeekNum;
            }
            if (iter->maxShiftNum > iter->maxConsecutiveDayNum) {
                iter->maxShiftNum_lastWeek = iter->maxConsecutiveDayNum +
                    ((iter->maxShiftNum - iter->maxConsecutiveDayNum) * history.pastWeekCount / scenario.totalWeekNum);
                iter->maxShiftNum = iter->maxConsecutiveDayNum +
                    ((iter->maxShiftNum - iter->maxConsecutiveDayNum) * history.currentWeek / scenario.totalWeekNum);
            }
        }
    }
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
    singleAssign = 0;   // due to no extra assignments
    underStaff = 0;     // due to no extra assignments
    succession = DefaultPenalty::FORBIDDEN_MOVE;
    missSkill = DefaultPenalty::FORBIDDEN_MOVE;

    insufficientStaff = 0;  // due to no extra assignments
    consecutiveShift = DefaultPenalty::ConsecutiveShift;
    consecutiveDay = DefaultPenalty::ConsecutiveDay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    preference = DefaultPenalty::Preference;
    completeWeekend = DefaultPenalty::CompleteWeekend;
    totalAssign = DefaultPenalty::TotalAssign;
    totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::setBlockSwapMode()
{
    singleAssign = 0;   // due to no extra assignments
    underStaff = 0;     // due to no extra assignments
    succession = 0; // due to it is calculated outside the try
    missSkill = 0;  // due to it is calculated outside the try

    insufficientStaff = 0;  // due to no extra assignments
    consecutiveShift = DefaultPenalty::ConsecutiveShift;
    consecutiveDay = DefaultPenalty::ConsecutiveDay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    preference = DefaultPenalty::Preference;
    completeWeekend = DefaultPenalty::CompleteWeekend;
    totalAssign = DefaultPenalty::TotalAssign;
    totalWorkingWeekend = DefaultPenalty::TotalWorkingWeekend;
}

void NurseRostering::Penalty::setExchangeMode()
{
    singleAssign = 0;   // due to no extra assignments
    underStaff = DefaultPenalty::FORBIDDEN_MOVE;
    succession = 0;     // due to it is calculated outside the try
    missSkill = 0;      // due to it is the same nurse

    insufficientStaff = DefaultPenalty::InsufficientStaff;
    consecutiveShift = DefaultPenalty::ConsecutiveShift;
    consecutiveDay = DefaultPenalty::ConsecutiveDay;
    consecutiveDayOff = DefaultPenalty::ConsecutiveDayOff;
    preference = DefaultPenalty::Preference;
    completeWeekend = DefaultPenalty::CompleteWeekend;
    totalAssign = 0;
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
