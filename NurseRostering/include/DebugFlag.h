/**
*   usage : 1. make the program print and record debug information.
*
*   note :  1.
*/

#ifndef INRC2_DEBUG_FLAG_H
#define INRC2_DEBUG_FLAG_H


// performance switch
// comment to reduce error messages and log
#define INRC2_LOG

// comment to reduce unnecessary calculation
#define INRC2_DEBUG

// comment to reduce console output
//#define INRC2_PERFORMANCE_TEST


// input switch
// uncomment to check the feasibility of the instance from the online feasible checker
//#define INRC2_CHECK_INSTANCE_FEASIBILITY_ONLINE


// algorithm switch
// comment to use random pick
#define INRC2_SECONDARY_OBJ_VALUE

// comment to use perturb
#define INRC2_PERTRUB_IN_REBUILD

// comment to use best improvement
#define INRC2_BLOCK_SWAP_FIRST_IMPROVE

#define INRC2_BLOCK_SWAP_AVERAGE_TABU 0
#define INRC2_BLOCK_SWAP_STRONG_TABU 1
#define INRC2_BLOCK_SWAP_WEAK_TABU 2
#define INRC2_BLOCK_SWAP_NO_TABU 3
#define INRC2_BLOCK_SWAP_TABU_STRENGTH INRC2_BLOCK_SWAP_NO_TABU

#define INRC2_BLOCK_SWAP_ORGN 0
#define INRC2_BLOCK_SWAP_FAST 1
#define INRC2_BLOCK_SWAP_PART 2
#define INRC2_BLOCK_SWAP_RAND 3
#define INRC2_BLOCK_SWAP_FIND_BEST INRC2_BLOCK_SWAP_FAST


#endif