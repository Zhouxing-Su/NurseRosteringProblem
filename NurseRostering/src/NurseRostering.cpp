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

const NurseRostering::SkillID NurseRostering::Scenario::Skill::ID_NONE = 0;
const NurseRostering::SkillID NurseRostering::Scenario::Skill::ID_BEGIN = NurseRostering::Scenario::Skill::ID_NONE + 1;



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
