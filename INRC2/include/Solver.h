/**
*   usage : 1. provide solver for Nurse Rostering Problem.
*
*   note :  1.  [optimizable] Solver has virtual function.
*           2.  set algorithm arguments in run().
*           3.  use a priority queue to manage available nurse when assigning?
*           v4. record 8 day which the first day is last day of last week to unify
*               the succession judgment.
*           x5. [optimizable] put evaluateObjValue() in initAssign() or call it in init() ?
*           6.  [optimizable] evaluateConsecutiveDay() and evaluateConsecutiveDayOff() can
*               be put together and not consider if there is an assignment. but there is
*               a difficult problem with consecutive state on the beginning of the week.
*           7.  [optimizable] make Weekday not related to Consecutive count from 0 to save space.
*           8.  [optimizable] make shiftID count from 1 and make Shift::ID_NONE 0,
*               this will leave out isWorking() in isValidSuccesion().
*               also, add a slot after Sun in assign with shift ID_NONE to
*               leave out (weekday >= Weekday::Sun) in isValidPrior()
*           9.  timeout may overflow in POSIX system since CLOCKS_PER_SEC is 1000000 which
*               makes clock_t can only count to about 4000 seconds.
*           10. record() uses c style function to check file open state, change it to c++ style.
*
*/

#ifndef TABUSOLVER_H
#define TABUSOLVER_H


#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <cstdio>

#include "DebugFlag.h"
#include "NurseRostering.h"
#include "Solution.h"
#include "utility.h"


class NurseRostering::Solver
{
public:
    static const int ILLEGAL_SOLUTION = -1;
    static const int CHECK_TIME_INTERVAL_MASK_IN_ITER = ((1 << 10) - 1);
    static const int RECORD_RETRY_INTERVAL = 50;// in millisecond
    static const clock_t SAVE_SOLUTION_TIME;    // 0.5 seconds
    static const clock_t REPAIR_TIMEOUT_IN_INIT;// 2 seconds

    const NurseRostering &problem;
    const clock_t startTime;


    Solver( const NurseRostering &input, clock_t startTime );
    virtual ~Solver() {}

    // set algorithm name, set parameters, generate initial solution
    virtual void init( const std::string &runID ) = 0;
    // search for optima
    virtual void solve() = 0;
    // generate history for next week
    virtual History genHistory() const;
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

    // calculate objective of optima with original input instead of auxiliary data structure
    // return objective value if solution is legal, else ILLEGAL_SOLUTION
    bool checkFeasibility( const AssignTable &assgin ) const;
    bool checkFeasibility() const;  // check optima assign
    ObjValue checkObjValue( const AssignTable &assign ) const;
    ObjValue checkObjValue() const;  // check optima assign

    const NurseNumOfSkill& getNurseNumOfSkill() const { return nurseNumOfSkill; }
    const NurseWithSkill& getNurseWithSkill() const { return nurseWithSkill; }

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

    Output optima;

    std::string runID;
    std::string algorithmName;

private:    // forbidden operators
    Solver& operator=(const Solver &) { return *this; }
};


class NurseRostering::TabuSolver : public NurseRostering::Solver
{
public:
    TabuSolver( const NurseRostering &input, clock_t startTime = clock() );
    virtual ~TabuSolver() {}

    virtual void init( const std::string &runID );
    virtual void solve();

private:
    void greedyInit();
    void exactInit();


    Solution sln;

private:    // forbidden operators
    TabuSolver& operator=(const TabuSolver &) { return *this; }
};


#endif