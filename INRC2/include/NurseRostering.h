/**
*   usage : 1. contain data for identifying a Nurse Rostering Problem.
*           2. provide solver interface.
*
*   note :  1.
*/

#ifndef NURSE_ROSTERING_H
#define NURSE_ROSTERING_H


#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <ctime>

#include "DebugFlag.h"
#include "utility.h"


class NurseRostering
{
public:
    static const int MAX_RUNNING_TIME = 1073741824;  // in clock count
    enum Weekday { HIS = 0, Mon, Tue, Wed, Thu, Fri, Sat, Sun, NUM = Sun, SIZE };
    enum Penalty
    {
        AMP = 1000,
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

    class SingleAssign
    {
    public:
        // the default constructor means there is no assignment
        SingleAssign( ShiftID sh = Scenario::Shift::ID_NONE, SkillID sk = 0 ) :shift( sh ), skill( sk ) {}

        ShiftID shift;
        SkillID skill;
    };
    // Assign[nurse][day] is a SingleAssign
    class Assign : public std::vector < std::vector< SingleAssign > >
    {
    public:
        Assign() {}
        Assign( int nurseNum, int weekdayNum = Weekday::SIZE, const SingleAssign &singleAssign = SingleAssign() )
            : std::vector< std::vector< SingleAssign > >( nurseNum, std::vector< SingleAssign >( weekdayNum, singleAssign ) ) {}

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


    class Solver
    {
    public:
        static const int ILLEGAL_SOLUTION = -1;
        static const int CHECK_TIME_INTERVAL_MASK_IN_ITER = ((1 << 10) - 1);
        static const clock_t SAVE_SOLUTION_TIME;    // 0.5 seconds
        static const clock_t REPAIR_TIMEOUT_IN_INIT;// 2 seconds

        class Output
        {
        public:
            Output() :objVal( -1 ) {}
            Output( int objValue, const Assign &assignment )
                :objVal( objValue ), assign( assignment ), findTime( clock() )
            {
            }

            Assign assign;
            ObjValue objVal;
            clock_t findTime;
        };

        // set algorithm name, set parameters, generate initial solution
        virtual void init() = 0;
        // search for optima
        virtual void solve() = 0;
        // generate history for next week
        virtual History genHistory() const = 0;
        // return const reference of the optima
        const Output& getOptima() const { return optima; }
        // print simple information of the solution to console
        void print() const;
        // record solution to specified file and create custom file if required
        void record( const std::string logFileName, const std::string &instanceName ) const;  // contain check()
        // return true if the optima solution is feasible and objValue is the same
        bool check() const;

        // calculate objective of optima with original input instead of auxiliary data structure
        // return objective value if solution is legal, else ILLEGAL_SOLUTION
        bool checkFeasibility( const Assign &assgin ) const;
        bool checkFeasibility() const;  // check optima assign
        ObjValue checkObjValue( const Assign &assign ) const;
        ObjValue checkObjValue() const;  // check optima assign


        Solver( const NurseRostering &input, const std::string &name, clock_t startTime );
        virtual ~Solver() {}


        const NurseRostering &problem;

    protected:
        // create header of the table ( require ios::app flag or "a" mode )
        static void initResultSheet( std::ofstream &csvFile );
        // solver can check termination condition every certain number of iterations
        // this determines if it is the right iteration to check time
        static bool isIterForTimeCheck( int iterCount ) // currently no use
        {
            return (!(iterCount & CHECK_TIME_INTERVAL_MASK_IN_ITER));
        }

        NurseNumsOnSingleAssign countNurseNums( const Assign &assign ) const;
        void checkConsecutiveViolation( int &objValue,
            const Assign &assign, NurseID nurse, int weekday, ShiftID lastShiftID,
            int &consecutiveShift, int &consecutiveDay, int &consecutiveDayOff,
            bool &shiftBegin, bool &dayBegin, bool &dayoffBegin ) const;


        Output optima;

        std::string algorithmName;
        clock_t startTime;

    private:    // forbidden operators
        Solver& operator=(const Solver &) { return *this; }
    };

    class TabuSolver;


    static const ObjValue MAX_OBJ_VALUE;


    // must set all data members by direct accessing!
    NurseRostering();


    // data to identify a nurse rostering problem
    int randSeed;
    clock_t timeout;    // time in clock count. 0 for just generate initial solution
    WeekData weekData;
    Scenario scenario;
    History history;
    Names names;

private:

};


#endif
