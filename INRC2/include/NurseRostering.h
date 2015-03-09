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
//#include <numeric>


class NurseRostering
{
public:
    typedef int ObjValue;   // unit of objective function
    typedef int NurseID;    // non-negative number for a certain nurse
    typedef int ContractID; // non-negative number for a certain contract
    typedef int ShiftID;    // NONE, ANY or non-negative number for a certain kind of shift
    typedef int SkillID;    // non-negative number for a certain kind of skill

    // NurseNum_Day_Shift_Skill[day][shift][skill] is a number of nurse
    typedef std::vector< std::vector< std::vector<int> > > NurseNum_Day_Shift_Skill;

    class Scenario
    {
    public:
        int maxWeekCount;
        int shiftTypeNum;
        int skillTypeNum;
        int nurseNum;

        class Shift
        {
        public:
            static const ShiftID ID_ANY = -1;
            static const std::string NAME_ANY;
            static const ShiftID ID_NONE = -2;
            static const std::string NAME_NONE;

            int minConsecutiveShiftNum;
            int maxConsecutiveShiftNum;
            // (illegalNextShift[nextShift] == true) means nextShift 
            // is available to be succession of this shift
            std::vector<bool> legalNextShifts;
        };
        std::vector<Shift> shifts;

        class Contract
        {
        public:
            int minShiftNum;    // total assignments in the planning horizon
            int maxShiftNum;    // total assignments in the planning horizon
            int minConsecutiveWorkingDayNum;
            int maxConsecutiveWorkingDayNum;
            int minConsecutiveDayoffNum;
            int maxConsecutiveDayoffNum;
            int maxWorkingWeekendNum;   // total assignments in the planning horizon
            bool completeWeekend;
        };
        std::vector<Contract> contracts;

        class Nurse
        {
        public:
            static const NurseID ID_NONE = -1;

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
        NurseNum_Day_Shift_Skill optNurseNums;
        // optNurseNums[day][shift][skill] is a number of nurse
        NurseNum_Day_Shift_Skill minNurseNums;
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
        SingleAssign( ShiftID sh = Scenario::Shift::ID_NONE, SkillID sk = 0 ) :shift( sh ), skill( sk ) {}

        ShiftID shift;
        SkillID skill;
    };
    // Assign[nurse][day] is a SingleAssign
    typedef std::vector< std::vector< SingleAssign > > Assign;

    class Solver
    {
    public:
        static const int ILLEGAL_SOLUTION = -1;

        class Output
        {
        public:
            Output() :objVal( -1 ) {}
            Output( int objValue, const Assign &assignment )
                :objVal( objValue ), assign( assignment )
            {
            }

            Assign assign;
            ObjValue objVal;
        };

        // set algorithm name, set parameters, generate initial solution
        virtual void init() = 0;
        // search for optima
        virtual void solve() = 0;
        // return const reference of the optima
        const Output& getOptima() const
        {
            return optima;
        }
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
        int checkObjValue( const Assign &assign ) const;
        int checkObjValue() const;  // check optima assign

        Solver( const NurseRostering &input, const std::string &name, clock_t startTime );
        virtual ~Solver();


        const NurseRostering &problem;

    protected:
        // create header of the table ( require ios::app flag or "a" mode )
        static void initResultSheet( std::ofstream &csvFile );

        NurseNum_Day_Shift_Skill countNurseNums( const Assign &assign ) const;

        Output optima;

        std::string algorithmName;
        clock_t startTime;
        clock_t endTime;
        int iterCount;
        int generationCount;
    };

    class TabuSolver : public Solver
    {
    public:
        static const std::string Name;

        virtual void init();
        virtual void solve();

        // initialize data about nurse-skill relation
        void discoverNurseSkillRelation();  // initialize nurseWithSkill, nurseNumOfSkill
        TabuSolver( const NurseRostering &input, clock_t startTime = clock() );
        virtual ~TabuSolver();

    private:
        // NurseWithSkill[skill][skillNum-1] is a set of nurses who have that skill and have skillNum skills in total
        typedef std::vector< std::vector<std::vector<NurseID> > > NurseWithSkill;

        class Solution
        {
        public:
            void resetAssign();   // reset assign
            bool genInitSln_random();
            void genNewHistory();

            // shift must not be none shift
            bool isValidSuccession( NurseID nurse, ShiftID shift, int weekday ) const;
            // require assign be initialized to Shift::ID_NONE
            bool isAssigned( NurseID nurse, int weekday ) const
            {
                return (assign[nurse][weekday].shift != NurseRostering::Scenario::Shift::ID_NONE);
            }

            Solution( TabuSolver &solver );
            operator Output() const
            {
                return Output( objValue, assign );
            }

        private:
            class AvailableNurses
            {
            public:
                // reset flags of available nurses
                // this method should be called before any other invoking
                void setEnvironment( int weekday, SkillID skill );
                // reset validNurseNum_CurShift
                // this method should be called before getNurse()
                void setShift( ShiftID shift );

                // always get an available nurse and update validation information
                NurseID getNurse();

                AvailableNurses( const Solution &s ) :sln( s ), nurseWithSkill( s.solver.nurseWithSkill ) {}
            private:
                const Solution &sln;
                NurseWithSkill nurseWithSkill;

                int weekday;
                ShiftID shift;
                SkillID skill;
                unsigned minSkillNum;
                std::vector<int> validNurseNum_CurShift;
                std::vector<int> validNurseNum_CurDay;
            };

            TabuSolver &solver;

            ObjValue objValue;
            Assign assign;
        };

        Solution sln;

        // nurseNumOfSkill[skill] is the number of nurses with that skill
        std::vector<int> nurseNumOfSkill;
        // nurseWithSkill[skill][skillNum-1] is a set of nurses who have that skill and have skillNum skills in total
        NurseWithSkill nurseWithSkill;
    };


    static const int WEEKDAY_NUM = 7;

    static const ObjValue MAX_OBJ_VALUE = 2000000000;
    static const int MAX_RUNNING_TIME = 1073741824;  // in millisecond


    // must set all data members by direct accessing!
    NurseRostering();


    // data to identify a nurse rostering problem
    int randSeed;
    int runningTime;    // time in millisecond
    int weekCount;      // count from 0 (the number in history file)
    WeekData weekData;
    Scenario scenario;
    History history;
    Names names;

private:

};


#endif
