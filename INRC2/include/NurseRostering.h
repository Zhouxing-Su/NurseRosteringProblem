/**
*   usage : 1. provide solver interface for Nurse Rostering Problem.
*
*   note :  1. [optimizable] Solver has virtual function.
*           2. set algorithm arguments in run().
*/

#ifndef NURSE_ROSTERING_H
#define NURSE_ROSTERING_H


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>
#include <map>


class NurseRostering
{
public:
    static const int WEEKDAY_NUM;
    static const int MAX_OBJ_VALUE;

    typedef int NurseID;    // non-negative number for a certain nurse
    typedef int ContractID; // non-negative number for a certain contract
    typedef int ShiftID;    // ? for NONE, ? for ANY, non-negative number for a certain kind of shift
    typedef int Skill;      // non-negative number for a certain kind of skill
    class Assign
    {
    public:
        ShiftID shift;
        Skill skill;
    };

    class Scenario
    {
    public:
        int weekNum;

        std::vector<std::string> skillNames; // skillName[skill]

        class Shift
        {
        public:
            static const ShiftID ShiftID_Any;
            static const std::string ShiftName_Any;
            static const ShiftID ShiftID_None;
            static const std::string ShiftName_None;

            std::string name;
            int minConsecutiveShiftNum;
            int maxConsecutiveShiftNum;
            // (illegalNextShift[nextShift] == true) means nextShift 
            // is forbidden to be succession of this shift
            std::vector<bool> illegalNextShifts;
        };
        std::vector<Shift> shifts;

        class Contract
        {
        public:
            std::string name;
            int minShiftNum;    // total assignments in the planning horizon
            int maxShiftNum;    // total assignments in the planning horizon
            int minConsecutiveWorkingDayNum;
            int maxConsecutiveWorkingDayNum;
            int minConsecutiveDayoffNum;
            int maxConsecutiveDayoffNum;
            int maxWorkingWeekendNum;
            bool completeWeekend;
        };
        std::vector<Contract> contracts;

        class Nurse
        {
        public:
            std::string name;
            ContractID contract;
            std::vector<Skill> skills;
        };
        std::vector<Nurse> nurses;
    };

    class WeekData
    {
    public:
        // (shiftOffs[day][shift][nurse] == true) means shiftOff required
        std::vector< std::vector< std::vector<bool> > > shiftOffs;
        // optNurseNums[day][shift][skill] is a number of nurse
        std::vector< std::vector< std::vector<int> > > optNurseNums;
        // optNurseNums[day][shift][skill] is a number of nurse
        std::vector< std::vector< std::vector<int> > > minNurseNums;
    };

    class NurseHistory
    {
    public:
        int shiftNum;
        int workingWeekendNum;
        ShiftID lastShift;
        int consecutiveShiftNum;
        int consecutiveWorkingDayNum;
        int consecutiveDayoffNum;
    };
    // history[nurse] is the history of a certain nurse
    typedef std::vector<NurseHistory> History;

    class Solver
    {
    public:
        class Input
        {
        public:
            Input()
            {
                shiftMap[NurseRostering::Scenario::Shift::ShiftName_Any] = NurseRostering::Scenario::Shift::ShiftID_Any;
                shiftMap[NurseRostering::Scenario::Shift::ShiftName_None] = NurseRostering::Scenario::Shift::ShiftID_None;
            }

            int weekCount;  // number of weeks that have past (the number in history file)
            WeekData weekData;
            Scenario scenario;
            History history;
            std::string solutionFileName;
            std::string customOutputFileName;
            int randSeed;

            // auxiliary data
            std::map<std::string, ShiftID> shiftMap;
            std::map<std::string, Skill> skillMap;
            std::map<std::string, NurseID> nurseMap;
            std::map<std::string, ContractID> contractMap;
        };

        class Output
        {
        public:
            Output() :objVal( -1 ) {}
            Output( int objValue, const std::vector< std::vector<Assign> > &assignments )
                :objVal( objValue ), assigns( assignments )
            {
            }

            // Output[day][nurse] is an Assign
            std::vector< std::vector<Assign> > assigns;
            int objVal;
        };

        virtual void init() = 0;
        virtual void solve() = 0;
        virtual int check() const = 0;
        virtual void print() const = 0;

        // log to file ( require ios::app flag or "a" mode )
        static void initResultSheet( std::ofstream &csvFile );
        void appendResultToSheet( const std::string &instanceFileName,
            std::ofstream &csvFile ) const;  // contain check()

        Solver( const Input &input, const std::string &algorithmName );
        virtual ~Solver();

    private:
        const WeekData weekData;
        const Scenario scenario;
        const History history;

        Output optima;

        const std::string solutionFileName;
        const std::string customOutputFileName;
        const int randSeed;
        const std::string algorithmName;
        clock_t startTime;
        clock_t endTime;
        int iterCount;
        int generationCount;
    };

    class TabuSolver : public Solver
    {
    public:
        virtual void init();
        virtual void solve();
        virtual int check() const;
        virtual void print() const;

        TabuSolver( const Input &input, const std::string &algorithmName );
        virtual ~TabuSolver();

    private:
        class Solution
        {
        public:

        private:

        };

        Solution sln;
    };

    NurseRostering();
    ~NurseRostering();

private:

};


#endif
