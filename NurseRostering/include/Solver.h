/**
*   usage : 1. provide solver for Nurse Rostering Problem.
*
*   note :  1.  [optimizable] Solver has virtual function.
*           v2.  set algorithm arguments in run().
*           3.  use a priority queue to manage available nurse when assigning?
*           v4. record 8 day which the first day is last day of last week to unify
*               the succession judgment.
*           x5. [optimizable] put evaluateObjValue() in initAssign() or call it in init() ?
*           6.  [optimizable] evaluateConsecutiveDay() and evaluateConsecutiveDayOff() can
*               be put together and not consider if there is an assignment. but there is
*               a difficult problem with consecutive state on the beginning of the week.
*           x7.  [optimizable] make Weekday not related to Consecutive count from 0 to save space.
*           v8.  [optimizable] make shiftID count from 1 and make Shift::ID_NONE 0,
*               this will leave out isWorking() in isValidSuccesion().
*               also, add a slot after Sun in assign with shift ID_NONE to
*               leave out (weekday >= Weekday::Sun) in isValidPrior()
*           9.  timeout may overflow in POSIX system since CLOCKS_PER_SEC is 1000000 which
*               makes clock_t can only count to about 4000 seconds.
*           v10. record() uses c style function to check file open state, change it to c++ style.
*           v11. reset solution to global optima or last local optima after each tabuSearch?
*           12. use constructor with assign string of AssignTable to rebuild solution.
*
*/

#ifndef TABUSOLVER_H
#define TABUSOLVER_H


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <cstdio>
#include <cmath>

#include "DebugFlag.h"
#include "NurseRostering.h"
#include "Solution.h"
#include "utility.h"


class NurseRostering::Solver
{
public:
    static const int CHECK_TIME_INTERVAL_MASK_IN_ITER = ((1 << 10) - 1);
    static const Timer::Duration SAVE_SOLUTION_TIME;    // 0.5 seconds

    // inverse possibility of starting perturb from optima in last search
    static const int PERTURB_ORIGIN_SELECT = 4;
    // inverse possibility of starting bias tabu search from optima in last search
    static const int BIAS_TS_ORIGIN_SELECT = 2;
    static const double INIT_PERTURB_STRENGTH;
    static const double PERTURB_STRENGTH_DELTA;
    static const double MAX_PERTURB_STRENGTH;

    enum InitAlgorithm
    {
        Greedy, Exact
    };

    enum SolveAlgorithm
    {
        RandomWalk, IterativeLocalSearch,
        TabuSearch_Possibility, TabuSearch_Loop, TabuSearch_Rand,
        BiasTabuSearch, SwapChainSearch
    };

    enum TabuTenureCoefficientIndex
    {
        TableSize, NurseNum, DayNum, ShiftNum, SIZE
    };

    class Config
    {
    public:
        Config() : initAlgorithm( InitAlgorithm::Greedy ),
            solveAlgorithm( SolveAlgorithm::TabuSearch_Rand ),
            modeSeq( Solution::ModeSeq::ACBR ),
            maxNoImproveCoefficient( 1 )
        {
            dayTabuCoefficient[TabuTenureCoefficientIndex::TableSize] = 0;
            dayTabuCoefficient[TabuTenureCoefficientIndex::NurseNum] = 0;
            dayTabuCoefficient[TabuTenureCoefficientIndex::DayNum] = 1;
            dayTabuCoefficient[TabuTenureCoefficientIndex::ShiftNum] = 0;

            shiftTabuCoefficient[TabuTenureCoefficientIndex::TableSize] = 0;
            shiftTabuCoefficient[TabuTenureCoefficientIndex::NurseNum] = 0;
            shiftTabuCoefficient[TabuTenureCoefficientIndex::DayNum] = 1;
            shiftTabuCoefficient[TabuTenureCoefficientIndex::ShiftNum] = 0;
        }

        InitAlgorithm initAlgorithm;
        SolveAlgorithm solveAlgorithm;
        Solution::ModeSeq modeSeq;
        double maxNoImproveCoefficient;
        double dayTabuCoefficient[TabuTenureCoefficientIndex::SIZE];
        double shiftTabuCoefficient[TabuTenureCoefficientIndex::SIZE];
    };


    static const std::vector<std::string> solveAlgorithmName;

    const NurseRostering &problem;
    const Timer::TimePoint startTime;
    const Timer timer;


    Solver( const NurseRostering &input, Timer::TimePoint startTime );
    Solver( const NurseRostering &input, const Output &optima, Timer::TimePoint startTime );
    virtual ~Solver() {}

    // set algorithm name, set parameters, generate initial solution
    virtual void init( const Config &cfg = Config(), const std::string &runID = std::string() ) = 0;
    // search for optima
    virtual void solve() = 0;
    // return true if global optima or population is updated
    virtual bool updateOptima( const Output &localOptima ) = 0;
    // generate history for next week
    virtual History genHistory() const = 0;
    // return const reference of the optima
    const Output& getOptima() const { return optima; }
    // print simple information of the solution to console
    void print() const;
    // record solution to specified file and create custom file if required
    void record( const std::string logFileName, const std::string &instanceName ) const;  // contain check()
    // log with time and runID
    void errorLog( const std::string &msg ) const;
    // return true if the optima solution is feasible and objValue is the same
    bool check() const;

    // use original input instead of auxiliary data structure
    // return 0 if no violation of hard constraints
    ObjValue checkFeasibility( const AssignTable &assgin ) const;
    ObjValue checkFeasibility() const;  // check optima assign
    // return objective value if solution is legal
    ObjValue checkObjValue( const AssignTable &assign ) const;
    ObjValue checkObjValue() const;  // check optima assign

    const NurseNumOfSkill& getNurseNumOfSkill() const { return nurseNumOfSkill; }
    const NurseWithSkill& getNurseWithSkill() const { return nurseWithSkill; }
    bool haveSameSkill( NurseID nurse, NurseID nurse2 ) const
    {
        return nursesHasSameSkill[nurse][nurse2];
    }


    mutable std::mt19937 randGen;

protected:
    // create header of the table ( require ios::app flag or "a" mode )
    static void initResultSheet( std::ofstream &csvFile );
    // solver can check termination condition every certain number of iterations
    // this determines if it is the right iteration to check time
    static bool isIterForTimeCheck( int iterCount ) // currently no use
    {
        return (!(iterCount & CHECK_TIME_INTERVAL_MASK_IN_ITER));
    }

    NurseNumsOnSingleAssign countNurseNums( const AssignTable &assign ) const;
    void checkConsecutiveViolation( int &objValue,
        const AssignTable &assign, NurseID nurse, int weekday, ShiftID lastShiftID,
        int &consecutiveShift, int &consecutiveDay, int &consecutiveDayOff,
        bool &shiftBegin, bool &dayBegin, bool &dayoffBegin ) const;

    // initialize assist data about nurse-skill relation
    void discoverNurseSkillRelation();


    // nurse-skill relation
    NurseNumOfSkill nurseNumOfSkill;
    NurseWithSkill nurseWithSkill;
    NursesHasSameSkill nursesHasSameSkill;


    Output optima;

    Config config;

    // information for log record
    std::string runID;
    std::string algorithmName;
    IterCount iterationCount;
    IterCount generationCount;

private:    // forbidden operators
    Solver& operator=(const Solver &) { return *this; }
};


class NurseRostering::TabuSolver : public NurseRostering::Solver
{
public:
    // minimal tabu tenure base
    static const int MIN_TABU_BASE = 6;
    // ratio of tabuTenureBase to tabuTenureAmp
    static const int TABU_BASE_TO_AMP = 4;
    // ratio of biased nurse number in total nurse number
    static const int INVERSE_TOTAL_BIAS_RATIO = 4;
    // ratio of biased nurse selected by penalty of each nurse
    static const int INVERSE_PENALTY_BIAS_RATIO = 5;


    TabuSolver( const NurseRostering &input, Timer::TimePoint startTime = Timer::Clock::now() );
    TabuSolver( const NurseRostering &input, const Output &optima, Timer::TimePoint startTime = Timer::Clock::now() );
    virtual ~TabuSolver() {}

    virtual void init( const Config &cfg = Config(), const std::string &runID = std::string() );
    virtual void solve();

    virtual bool updateOptima( const Output &localOptima );
    virtual History genHistory() const;

    void checkDump( std::string assignString )
    {
        sln.rebuild( Output( 0, AssignTable( problem.scenario.nurseNum, Weekday::SIZE, assignString ) ) );
        sln.evaluateObjValue( false );
        sln.evaluateObjValue();
        checkFeasibility( sln.getAssignTable() );
        checkObjValue( sln.getAssignTable() );
    }

    IterCount DayTabuTenureBase() const { return dayTabuTenureBase; }
    IterCount DayTabuTenureAmp() const { return dayTabuTenureAmp; }
    IterCount ShiftTabuTenureBase() const { return shiftTabuTenureBase; }
    IterCount ShiftTabuTenureAmp() const { return shiftTabuTenureAmp; }

    IterCount MaxNoImproveForSingleNeighborhood() const { return maxNoImproveForSingleNeighborhood; }
    IterCount MaxNoImproveForAllNeighborhood() const { return maxNoImproveForAllNeighborhood; }
    IterCount MaxNoImproveForBiasTabuSearch() const { return maxNoImproveForBiasTabuSearch; }
    IterCount MaxNoImproveSwapChainLength() const { return maxNoImproveSwapChainLength; }
    IterCount MaxSwapChainRestartCount() const { return maxSwapChainRestartCount; }

private:
    void greedyInit();
    void exactInit();

    // search with tabu search and swap chain search by turn
    void swapChainSearch( Solution::ModeSeq modeSeq );
    // turn the objective to optimize a subset of nurses when no improvement
    void biasTabuSearch( Solution::ModeSeq modeSeq );
    // search with tabu table
    void tabuSearch( Solution::ModeSeq modeSeq, Solution::TabuSearch search, IterCount maxNoImproveCount );
    // iteratively run local search and perturb
    void iterativeLocalSearch( Solution::ModeSeq modeSeq );
    // random walk until timeout
    void randomWalk();

    // set tabu tenure according to certain feature
    void setTabuTenure();
    // set it multiple times will multiply the tenure
    // pass in 0 to leave out the relation
    void setDayTabuTenure_TableSize( double coefficient );
    void setShiftTabuTenure_TableSize( double coefficient );
    void setDayTabuTenure_NurseNum( double coefficient );
    void setShiftTabuTenure_NurseNum( double coefficient );
    void setDayTabuTenure_DayNum( double coefficient );
    void setShiftTabuTenure_DayNum( double coefficient );
    void setDayTabuTenure_ShiftNum( double coefficient );
    void setShiftTabuTenure_ShiftNum( double coefficient );

    // set the max no improve count
    void setMaxNoImprove( double coefficient )
    {
        std::ostringstream oss;
        oss << "[MNI=" << coefficient << "]";

        algorithmName += oss.str();

        maxNoImproveForSingleNeighborhood = static_cast<IterCount>(
            coefficient * problem.scenario.nurseNum * Weekday::NUM);
        maxNoImproveForAllNeighborhood = static_cast<IterCount>(
            coefficient * problem.scenario.nurseNum * Weekday::NUM *
            sqrt( problem.scenario.shiftTypeNum * problem.scenario.skillTypeNum ));
        maxNoImproveForBiasTabuSearch = maxNoImproveForSingleNeighborhood / INVERSE_TOTAL_BIAS_RATIO;
        maxNoImproveSwapChainLength = maxNoImproveForSingleNeighborhood;
        maxSwapChainRestartCount = static_cast<IterCount>(sqrt( problem.scenario.nurseNum ));
    }

    IterCount dayTabuTenureBase;
    IterCount dayTabuTenureAmp;
    IterCount shiftTabuTenureBase;
    IterCount shiftTabuTenureAmp;

    IterCount maxNoImproveForSingleNeighborhood;
    IterCount maxNoImproveForAllNeighborhood;
    IterCount maxNoImproveForBiasTabuSearch;
    IterCount maxNoImproveSwapChainLength;
    IterCount maxSwapChainRestartCount;

    Solution sln;

private:    // forbidden operators
    TabuSolver& operator=(const TabuSolver &) { return *this; }
};


#endif
