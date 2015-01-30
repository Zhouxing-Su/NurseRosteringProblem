/**
*   usage : 1. set algorithm arguments in run()
*
*   note :  1. [optimizable] Solver has virtual function.
*/

#ifndef NURSE_ROSTERING_H
#define NURSE_ROSTERING_H


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>


class NurseRostering
{
public:
    static const int DAY_NUM;
    static const int MAX_OBJ_VALUE;

    typedef int ShiftID;  // ? for NONE, ? for ANY, positive number for a certain kind of shift
    typedef int Skill;    // non-negative number for a certain kind of skill
    struct Assign
    {
        ShiftID shift;
        Skill skill;
    };
    typedef int ContractID;

    struct Scenario
    {
        int weekNum;

        std::vector<std::string> skillNames; // skillName[skill]

        struct Shift
        {
            std::string name;
            int minConsecutiveShiftNum;
            int maxConsecutiveShiftNum;
            // (illegalNextShift[nextShift] == true) means nextShift 
            // is forbidden to be succession of this shift
            std::vector<bool> illegalNextShifts;
        };
        std::vector<Shift> shifts;

        struct Contract
        {
            std::string name;
            int minShiftNum;
            int maxShiftNum;
            int minConsecutiveWorkingDayNum;
            int maxConsecutiveWorkingDayNum;
            int minConsecutiveDayoffNum;
            int maxConsecutiveDayoffNum;
            int maxWorkingWeekendNum;
            bool completeWeekend;
        };
        std::vector<Contract> contracts;

        struct Nurse
        {
            std::string name;
            ContractID contract;
            std::vector<Skill> skills;
        };
        std::vector<Nurse> nurses;
    };

    struct WeekData
    {
        // shiftOffs[day][nurse] is a shift
        std::vector< std::vector<ShiftID> > shiftOffs;
        // optNurseNums[day][shift][skill] is a number of nurse
        std::vector< std::vector< std::vector<int> > > optNurseNums;
        // optNurseNums[day][shift][skill] is a number of nurse
        std::vector< std::vector< std::vector<int> > > minNurseNums;
    };

    struct NurseHistory
    {
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
        struct Input
        {
        public:
            WeekData weekData;
            Scenario scenario;
            History history;
            std::string solutionFileName;
            std::string customOutputFileName;
            int randSeed;
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
