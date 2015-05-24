/**
*   usage : 1. small independent modules.
*
*   note :  1.
*/

#ifndef UTILITY_H
#define UTILITY_H


#include <string>
#include <random>
#include <chrono>

#include "DebugFlag.h"


//
// -------+--------+------
//       low      high
//
// if target is less than low, the distance is (low - target),
// if target is greater than high, the distance is (target - high)
// else, the distance is 0
// 
// require operator-() and operator<()
template <typename T>
T distanceToRange( const T &target, const T &lowerBound, const T &upperBound )
{
    T dist = lowerBound - target;
    if (dist < 0) {
        dist = target - upperBound;
        if (dist < 0) {
            dist = 0;
        }
    }
    return dist;
}

//
// ---------+-----------
//        bound---->
//
// if target is greater than bound, the distance is (target - bound)
// else, the distance is 0
// 
// require operator-() and operator<()
template <typename T>
T exceedCount( const T &target, const T &bound )
{
    T diff( target - bound );
    return (0 < diff) ? diff : 0;
}

//
// ---------+-----------
//   <----bound
//
// if target is less than bound, the distance is (bound - target)
// else, the distance is 0
// 
// require operator-() and operator<()
template <typename T>
T absentCount( const T &target, const T &bound )
{
    T diff( bound - target );
    return (0 < diff) ? diff : 0;
}

//
class Timer
{
public:
    typedef std::chrono::steady_clock Clock;
    typedef std::chrono::milliseconds Duration;
    typedef std::chrono::steady_clock::time_point TimePoint;

    Timer( Duration duration, TimePoint startTime = Clock::now() )
        : endTime( startTime + duration )
    {
    }

    bool isTimeOut() const
    {
        return (Clock::now() >= endTime);
    }

    Duration restTime() const
    {
        return std::chrono::duration_cast<Duration>(
            endTime - Clock::now());
    }

private:
    TimePoint endTime;
};

//
template <typename T>
class RandSelect
{
public:
    // sometimes the first element is pre-selected with the possibility of 1,
    // so you can pass 1 into the constructor in this condition to leave out a isSelected() call.
    RandSelect( int startCount = 1 ) : count( startCount ) {}

    // start a new selection on another N elements
    void reset( int startCount = 1 )
    {
        count = startCount;
    }

    // call this for each of the N elements (N times in total) to judge whether each of them is selected.
    bool isSelected( std::mt19937 &randGen )
    {   // only the last returned "true" means that element is selected finally.
        return ((randGen() % (++count)) == 0);
    }

    bool isMinimal( const T& target, const T& min, std::mt19937 &randGen )
    {
        if (target < min) {
            reset();
            return true;
        } else if (target == min) {
            return isSelected( randGen );
        } else {
            return false;
        }
    }

private:
    int count;
};



#endif