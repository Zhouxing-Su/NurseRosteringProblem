/**
*   usage : 1.  make the program print and record debug information.
*           2.  [fix] indicate that the switch state is proved to
*               be better and should not be modified.
*
*   note :  1.
*/

#ifndef INRC2_DEBUG_FLAG_H
#define INRC2_DEBUG_FLAG_H


// performance switch
// comment to reduce error messages and log
#ifndef INRC2_LOG
//#define INRC2_LOG
#endif

// comment to reduce unnecessary calculation
#ifndef INRC2_DEBUG
//#define INRC2_DEBUG
#endif

// comment to reduce console output
#ifndef INRC2_PERFORMANCE_TEST
//#define INRC2_PERFORMANCE_TEST
#endif


// input switch
// uncomment to check the feasibility of the instance from the online feasible checker
#ifndef INRC2_CHECK_INSTANCE_FEASIBILITY_ONLINE
//#define INRC2_CHECK_INSTANCE_FEASIBILITY_ONLINE
#endif


// algorithm switch
// comment to abandon tabu
#ifndef INRC2_USE_TABU
#define INRC2_USE_TABU  // repair() has simple move only, will no tabu effect its execution?
#endif

// [fix] comment to start to consider min shift at the first week
#ifndef INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
#define INRC2_IGNORE_MIN_SHIFT_IN_EARLY_WEEKS
#endif

// comment to use rest max working weekend
#ifndef INRC2_AVERAGE_MAX_WORKING_WEEKEND
#define INRC2_AVERAGE_MAX_WORKING_WEEKEND
#endif

// [fix] comment to use random pick
#ifndef INRC2_SECONDARY_OBJ_VALUE
#define INRC2_SECONDARY_OBJ_VALUE
#endif

// [fix] comment to use a fixed delta of perturb strength 
#ifndef INRC2_INC_PERTURB_STRENGTH_DELTA
#define INRC2_INC_PERTURB_STRENGTH_DELTA
#endif

// [fix] comment to use perturb
#ifndef INRC2_PERTRUB_IN_REBUILD
#define INRC2_PERTRUB_IN_REBUILD
#endif


#endif