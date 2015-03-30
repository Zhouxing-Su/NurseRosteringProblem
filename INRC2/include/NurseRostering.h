/**
*   usage : 1. contain data for identifying a Nurse Rostering Problem.
*           2. provide solver interface.
*
*   note :  1. move DefaultPenalty into Penalty and change enum to ObjValue.
*/

#ifndef NURSE_ROSTERING_H
#define NURSE_ROSTERING_H


#include <map>
#include <vector>
#include <string>
#include <ctime>

#include "DebugFlag.h"


class NurseRostering
{
public:
    enum Weekday { HIS = 0, Mon, Tue, Wed, Thu, Fri, Sat, Sun, NUM = Sun, SIZE };
    enum DefaultPenalty
    {
        MAX_OBJ_VALUE = (1 << 30),
        // amplifier for improving accuracy
        AMP = 2 * 2 * 2 * 5 * 5,
        // attenuation for fast repair without considering quality
        DECAY = 5,
        // hard constraints
        SingleAssign = MAX_OBJ_VALUE,
        UnderStaff = MAX_OBJ_VALUE,
        UnderStaff_Repair = (AMP * 240),
        Succession = MAX_OBJ_VALUE,
        Succession_Repair = (AMP * 300),
        MissSkill = MAX_OBJ_VALUE,
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
        int shiftTypeNum;
        int skillTypeNum;
        int nurseNum;

        class Shift
        {
        public:
            static const ShiftID ID_ANY;
            static const std::string NAME_ANY;
            static const ShiftID ID_NONE;
            static const std::string NAME_NONE;

            int minConsecutiveShiftNum;
            int maxConsecutiveShiftNum;
            // (legalNextShift[nextShift] == true) means nextShift 
            // is available to be succession of this shift
            std::vector<bool> legalNextShifts;
        };
        std::vector<Shift> shifts;

        class Contract
        {
        public:
            int minShiftNum;    // total assignments in the planning horizon
            int maxShiftNum;    // total assignments in the planning horizon
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
        int pastWeekCount;  // count from 0 (the number in history file)
        int currentWeek;    // count from 1

        std::vector<int> totalAssignNums;
        std::vector<int> totalWorkingWeekendNums;
        std::vector<ShiftID> lastShifts;
        std::vector<int> consecutiveShiftNums;
        std::vector<int> consecutiveDayNums;
        std::vector<int> consecutiveDayoffNums;
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
        Assign( ShiftID sh = Scenario::Shift::ID_NONE, SkillID sk = 0 ) :shift( sh ), skill( sk ) {}

        ShiftID shift;
        SkillID skill;
    };
    // AssignTable[nurse][day] is a SingleAssign
    class AssignTable : public std::vector < std::vector< Assign > >
    {
    public:
        AssignTable() {}
        AssignTable( int nurseNum, int weekdayNum = Weekday::SIZE, const Assign &singleAssign = Assign() )
            : std::vector< std::vector< Assign > >( nurseNum, std::vector< Assign >( weekdayNum, singleAssign ) ) {}

        static bool isWorking( ShiftID shift )
        {
            return (shift != NurseRostering::Scenario::Shift::ID_NONE);
        }

        bool isWorking( NurseID nurse, int weekday ) const
        {
            return isWorking( at( nurse ).at( weekday ).shift );
        }
    private:

    };

    class Penalty
    {
    public:
        Penalty() { setNormalMode(); }

        // hard constraints must be satisfied 
        // and soft constraints get their original penalty
        void setNormalMode();
        // UnderStaff and InsufficientStaff is not considered
        // due to nurse number will not change
        void setSwapMode();
        // allow hard constraints UnderStaff and Succession being violated
        // but with much greater penalty than soft constraints
        void setRepairMode( ObjValue softConstraintDecay = DefaultPenalty::DECAY,
            ObjValue WeightOnUnderStaff = DefaultPenalty::UnderStaff_Repair,
            ObjValue WeightOnSuccesion = DefaultPenalty::Succession_Repair );

        // hard constraint
        ObjValue getUnderStaff() const { return UnderStaff; }
        ObjValue getSingleAssign() const { return SingleAssign; }
        ObjValue getSuccession() const { return Succession; };
        ObjValue getMissSkill() const { return MissSkill; }

        // soft constraint
        ObjValue getInsufficientStaff() const { return InsufficientStaff; }
        ObjValue getConsecutiveShift() const { return ConsecutiveShift; }
        ObjValue getConsecutiveDay() const { return ConsecutiveDay; }
        ObjValue getConsecutiveDayOff() const { return ConsecutiveDayOff; }
        ObjValue getPreference() const { return Preference; }
        ObjValue getCompleteWeekend() const { return CompleteWeekend; }
        ObjValue getTotalAssign() const { return TotalAssign; }
        ObjValue getTotalWorkingWeekend() const { return TotalWorkingWeekend; }

    private:
        // hard constraint
        ObjValue SingleAssign;
        ObjValue UnderStaff;
        ObjValue Succession;
        ObjValue MissSkill;

        // soft constraint
        ObjValue InsufficientStaff;
        ObjValue ConsecutiveShift;
        ObjValue ConsecutiveDay;
        ObjValue ConsecutiveDayOff;
        ObjValue Preference;
        ObjValue CompleteWeekend;
        ObjValue TotalAssign;
        ObjValue TotalWorkingWeekend;
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


    static const clock_t MAX_RUNNING_TIME;  // in clock count


    // must set all data members by direct accessing!
    NurseRostering();


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
