/**
*   usage : 1. contain data for identifying a Nurse Rostering Problem.
*           2. provide solver interface.
*
*   note :  v1. move DefaultPenalty into Penalty and change enum to ObjValue.
*/

#ifndef NURSE_ROSTERING_H
#define NURSE_ROSTERING_H


#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <ctime>

#include "DebugFlag.h"


class NurseRostering
{
public:
    enum Weekday { HIS = 0, Mon, Tue, Wed, Thu, Fri, Sat, Sun, NUM = Sun, NEXT_WEEK, SIZE };
    enum DefaultPenalty
    {
        // (delta >= MAX_OBJ_MAX) stands for forbidden move
        // a tryMove should return FORBIDDEN_MOVE in case it is used by other complex move
        MAX_OBJ_VALUE = (1 << 24),
        FORBIDDEN_MOVE = (MAX_OBJ_VALUE * 2),
        // amplifier for improving accuracy
        AMP = 2 * 2 * 2 * 3 * 5,
        // hard constraints
        SingleAssign = FORBIDDEN_MOVE,
        UnderStaff = FORBIDDEN_MOVE,
        Succession = FORBIDDEN_MOVE,
        MissSkill = FORBIDDEN_MOVE,

        UnderStaff_Repair = (AMP * 80),
        Succession_Repair = (AMP * 120),
        // soft constraints
        InsufficientStaff = (AMP * 30),
        ConsecutiveShift = (AMP * 15),
        ConsecutiveDay = (AMP * 30),
        ConsecutiveDayOff = (AMP * 30),
        Preference = (AMP * 10),
        CompleteWeekend = (AMP * 30),
        TotalAssign = (AMP * 20),
        TotalWorkingWeekend = (AMP * 30)
    };


    typedef int IterCount;  // iteration count for meta heuristic solver

    typedef int ObjValue;   // unit of objective function
    typedef int NurseID;    // non-negative number for a certain nurse
    typedef int ContractID; // non-negative number for a certain contract
    typedef int ShiftID;    // NONE, ANY or non-negative number for a certain kind of shift
    typedef int SkillID;    // non-negative number for a certain kind of skill

    // NurseNumsOnSingleAssign[day][shift][skill] is a number of nurse
    typedef std::vector< std::vector< std::vector<int> > > NurseNumsOnSingleAssign;

    class Scenario
    {
    public:
        // if there are weekNum weeks in the planning horizon, maxWeekCount = (weekNum - 1) 
        int maxWeekCount;   // count from 0
        int totalWeekNum;   // count from 1
        int shiftTypeNum;   // actual type number
        int shiftSize;      // type number in data structure
        int skillTypeNum;   // actual type number
        int skillSize;      // type number in data structure
        int nurseNum;

        class Shift
        {
        public:
            static const ShiftID ID_ANY;
            static const std::string NAME_ANY;
            static const ShiftID ID_NONE;
            static const std::string NAME_NONE;
            static const ShiftID ID_BEGIN;

            int minConsecutiveShiftNum;
            int maxConsecutiveShiftNum;
            // (legalNextShift[nextShift] == true) means nextShift 
            // is available to be succession of this shift
            std::vector<bool> legalNextShifts;
        };
        std::vector<Shift> shifts;

        class Skill
        {
        public:
            static const SkillID ID_NONE;
            static const SkillID ID_BEGIN;
        };

        class Contract
        {
        public:
            int minShiftNum;    // total assignments until current week
            int minShiftNum_lastWeek;   // generate according to history
            int maxShiftNum;    // total assignments until current week
            int maxShiftNum_lastWeek;   // generate according to history
            int minConsecutiveDayNum;
            int maxConsecutiveDayNum;
            int minConsecutiveDayoffNum;
            int maxConsecutiveDayoffNum;
            int maxWorkingWeekendNum;   // total assignments in the planning horizon
            bool completeWeekend;
        };
        std::vector<Contract> contracts;

        class Nurse
        {
        public:
            static const NurseID ID_NONE;

            ContractID contract;
            int skillNum;
            // (skills[skill] == true) means the nurse have that skill
            std::vector<bool> skills;
        };
        std::vector<Nurse> nurses;
    };

    class WeekData
    {
    public:
        // (shiftOffs[day][shift][nurse] == true) means shiftOff required
        std::vector< std::vector< std::vector<bool> > > shiftOffs;
        // optNurseNums[day][shift][skill] is a number of nurse
        NurseNumsOnSingleAssign optNurseNums;
        // optNurseNums[day][shift][skill] is a number of nurse
        NurseNumsOnSingleAssign minNurseNums;
    };

    // history.XXX[nurse] is the XXX of a certain nurse
    class History
    {
    public:
        ObjValue accObjValue;   // accumulated objective value
        int pastWeekCount;      // count from 0 (the number in history file)
        int currentWeek;        // count from 1

        std::vector<int> totalAssignNums;
        std::vector<int> totalWorkingWeekendNums;
        std::vector<ShiftID> lastShifts;
        std::vector<int> consecutiveShiftNums;
        std::vector<int> consecutiveDayNums;
        std::vector<int> consecutiveDayoffNums;

        bool ignoreMinShiftConstraint;  // generated after reading history for initializing minShiftNum_lastWeek next week
    };

    class Names
    {
    public:
        std::string scenarioName;

        std::vector<std::string> skillNames;    // skillMap[skillNames[skillID]] == skillID
        std::map<std::string, SkillID> skillMap;

        std::vector<std::string> shiftNames;    // shiftMap[shiftNames[shiftID]] == shiftID
        std::map<std::string, ShiftID> shiftMap;

        std::vector<std::string> contractNames; // contractMap[contractNames[contractID]] == contractID
        std::map<std::string, ContractID> contractMap;

        std::vector<std::string> nurseNames;    // nurseMap[nurseNames[nurseID]] == nurseID
        std::map<std::string, NurseID> nurseMap;
    };

    class Assign
    {
    public:
        // the default constructor means there is no assignment
        Assign( ShiftID sh = Scenario::Shift::ID_NONE, SkillID sk = NurseRostering::Scenario::Skill::ID_NONE ) :shift( sh ), skill( sk ) {}

        static bool isWorking( ShiftID shift )
        {
            return (shift != NurseRostering::Scenario::Shift::ID_NONE);
        }

        bool isWorking() const
        {
            return (shift != NurseRostering::Scenario::Shift::ID_NONE);
        }

        ShiftID shift;
        SkillID skill;
    };
    // AssignTable[nurse][day] is a SingleAssign
    class AssignTable : public std::vector < std::vector< Assign > >
    {
    public:
        AssignTable() {}
        // weekdayNum should be (actual weekday number + 1)
        // to let it allocate an additional day for history
        AssignTable( int nurseNum, int weekdayNum = Weekday::SIZE, const Assign &singleAssign = Assign() )
            : std::vector< std::vector< Assign > >( nurseNum, std::vector< Assign >( weekdayNum, singleAssign ) ) {}
        AssignTable( int nurseNum, int weekdayNum, const std::string &assignString )
            : std::vector< std::vector< Assign > >( nurseNum, std::vector< Assign >( weekdayNum ) )
        {
            std::istringstream iss( assignString );

            for (NurseID nurse = 0; nurse < nurseNum; ++nurse) {
                for (int weekday = Weekday::Mon; weekday < weekdayNum; ++weekday) {
                    iss >> at( nurse ).at( weekday ).shift
                        >> at( nurse ).at( weekday ).skill;
                }
            }
        }

        bool isWorking( NurseID nurse, int weekday ) const
        {
            return Assign::isWorking( at( nurse ).at( weekday ).shift );
        }

    private:

    };

    class Penalty
    {
    public:
        Penalty() { setDefaultMode(); }

        // hard constraints must be satisfied 
        // and soft constraints get their original penalty
        void setDefaultMode();
        // UnderStaff and InsufficientStaff is not considered
        // due to nurse number will not change on each shift
        void setSwapMode();
        // 
        void setBlockSwapMode();
        // TotalAssign is not considered due to total assign will not change
        // succession should be 
        void setExchangeMode();
        // allow hard constraints UnderStaff and Succession being violated
        // but with much greater penalty than soft constraints
        // set softConstraintDecay to MAX_OBJ_VALUE to make them does not count
        void setRepairMode( ObjValue WeightOnUnderStaff = DefaultPenalty::UnderStaff_Repair,
            ObjValue WeightOnSuccesion = DefaultPenalty::Succession_Repair,
            ObjValue softConstraintDecay = DefaultPenalty::MAX_OBJ_VALUE );

        // hard constraint
        ObjValue UnderStaff() const { return underStaff; }
        ObjValue SingleAssign() const { return singleAssign; }
        ObjValue Succession() const { return succession; };
        ObjValue MissSkill() const { return missSkill; }

        // soft constraint
        ObjValue InsufficientStaff() const { return insufficientStaff; }
        ObjValue ConsecutiveShift() const { return consecutiveShift; }
        ObjValue ConsecutiveDay() const { return consecutiveDay; }
        ObjValue ConsecutiveDayOff() const { return consecutiveDayOff; }
        ObjValue Preference() const { return preference; }
        ObjValue CompleteWeekend() const { return completeWeekend; }
        ObjValue TotalAssign() const { return totalAssign; }
        ObjValue TotalWorkingWeekend() const { return totalWorkingWeekend; }

    private:
        // hard constraint
        ObjValue singleAssign;
        ObjValue underStaff;
        ObjValue succession;
        ObjValue missSkill;

        // soft constraint
        ObjValue insufficientStaff;
        ObjValue consecutiveShift;
        ObjValue consecutiveDay;
        ObjValue consecutiveDayOff;
        ObjValue preference;
        ObjValue completeWeekend;
        ObjValue totalAssign;
        ObjValue totalWorkingWeekend;
    };

    class Output;
    class Solution;

    class Solver;
    class TabuSolver;


    // nurseNumOfSkill[skill] is the number of nurses with that skill
    typedef std::vector<SkillID> NurseNumOfSkill;
    // NurseWithSkill[skill][skillNum-1] is a set of nurses 
    // who have that skill and have skillNum skills in total
    typedef std::vector< std::vector<std::vector<NurseID> > > NurseWithSkill;
    // NursesHasSameSkill[nurse][nurse2] == true if both of them has certain skill
    typedef std::vector< std::vector<bool> > NursesHasSameSkill;


    static const clock_t MAX_RUNNING_TIME;  // in clock count


    // must set all data members by direct accessing!
    NurseRostering();

    bool hasSameSkill( NurseID nurse, NurseID nurse2 ) const
    {
        for (SkillID sk = Scenario::Skill::ID_BEGIN; sk < scenario.skillSize; ++sk) {
            if (scenario.nurses[nurse].skills[sk] && scenario.nurses[nurse2].skills[sk]) {
                return true;
            }
        }

        return false;
    }


    // data to identify a nurse rostering problem
    int randSeed;
    clock_t timeout;    // time in clock count. 0 for just generate initial solution
    WeekData weekData;
    Scenario scenario;
    History history;
    Names names;

private:    // forbidden operators
    NurseRostering& operator=(const NurseRostering&) { return *this; }
};


#endif
