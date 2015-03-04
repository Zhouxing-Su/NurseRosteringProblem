/**
*   usage : 1. provide solver interface for Nurse Rostering Problem.
*
*   note :  1. [optimizable] Solver has virtual function.
*           2. set algorithm arguments in run().
*           3. use a priority queue to manage available nurse when assigning?
*           4. record 8 day which the first day is last day of last week to unify
*              the succession judgment.
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
#include <set>
#include <algorithm>


class NurseRostering
{
public:
    static const int WEEKDAY_NUM;
    static const int MAX_OBJ_VALUE;

    typedef int NurseID;    // non-negative number for a certain nurse
    typedef int ContractID; // non-negative number for a certain contract
    typedef int ShiftID;    // ? for NONE, ? for ANY, non-negative number for a certain kind of shift
    typedef int SkillID;      // non-negative number for a certain kind of skill
    // Assign[day][shift][skill] is a set of nurses
    typedef std::vector< std::vector< std::vector< std::set<NurseID> > > > Assign;

    class Scenario
    {
    public:
        int weekNum;
        int shiftTypeNum;
        int skillTypeNum;
        int nurseNum;

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
            std::vector<SkillID> skills;
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
        class Output
        {
        public:
            Output() :objVal( -1 ) {}
            Output( int objValue, const Assign &assignment )
                :objVal( objValue ), assign( assignment )
            {
            }

            Assign assign;
            int objVal;
        };

        // set algorithm name, set parameters, generate initial solution
        virtual void init() = 0;
        // search for optima
        virtual void solve() = 0;
        // calculate objective with original input instead of auxiliary data structure
        virtual int check() const = 0;
        // print simple information of the solution to console
        virtual void print() const = 0;
        // record solution to specified file
        virtual void record() const = 0;

        // log to file ( require ios::app flag or "a" mode )
        static void initResultSheet( std::ofstream &csvFile );
        void appendResultToSheet( const std::string &instanceName,
            const std::string logFileName ) const;  // contain check()

        Solver( const NurseRostering &input );
        virtual ~Solver();


    public:
        const WeekData& getWeekData() const { return weekData; }
        const NurseHistory& getHistory( NurseID nurse ) const { return history[nurse]; }
        const Scenario scenario;

    protected:
        WeekData weekData;
        History history;

        Output optima;

        const std::string solutionFileName;
        const std::string customOutputFileName;
        const int randSeed;
        std::string algorithmName;
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
        virtual void record() const;

        // initialize data about nurse-skill relation
        void discoverNurseSkillRelation();  // initialize nurseWithSkill, nurseNumOfSkill
        TabuSolver( const NurseRostering &input );
        virtual ~TabuSolver();

    private:
        class Solution
        {
        public:
            void genInitSln_random();
            bool isAvailableAssign( NurseID nurse, ShiftID shift, int weekday ) const;

            Solution( TabuSolver &solver );

        private:
            TabuSolver &solver;
            Assign assign;
            History newHistory; // information of this week which affect next week
        };

        Solution sln;

        // nurseNumOfSkill[skill] is the number of nurses with that skill
        std::vector<int> nurseNumOfSkill;
        // nurseWithSkill[skill] is a set of nurses who have that skill
        std::vector<std::vector<NurseID> > nurseWithSkill;
    };


    NurseRostering();


    // data to identify a nurse rostering problem
    std::string solutionFileName;
    std::string customOutputFileName;
    int randSeed;
    int weekCount;  // number of weeks that have past (the number in history file)
    WeekData weekData;
    Scenario scenario;
    History history;

    // auxiliary data
    std::map<std::string, ShiftID> shiftMap;
    std::map<std::string, SkillID> skillMap;
    std::map<std::string, NurseID> nurseMap;
    std::map<std::string, ContractID> contractMap;

private:

};


#endif
