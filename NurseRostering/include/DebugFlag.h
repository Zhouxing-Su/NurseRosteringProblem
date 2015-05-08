/**
*   usage : 1. make the program print and record debug information.
*           2. comment the INRC2_LOG to reduce error messages and log.
*           3. comment the INRC2_PERFORMANCE_TEST to reduce console output.
*           4. uncomment the INRC2_CHECK_INSTANCE_FEASIBILITY_ONLINE to
*               check the feasibility of the instance from the online feasible checker.
*           5. comment the INRC2_DEBUG to reduce unnecessary calculation.
*
*   note :  1.
*/

#ifndef INRC2_DEBUG_FLAG_H
#define INRC2_DEBUG_FLAG_H


// performance switch
#define INRC2_LOG

#define INRC2_DEBUG

//#define INRC2_PERFORMANCE_TEST


// input switch
//#define INRC2_CHECK_INSTANCE_FEASIBILITY_ONLINE


// algorithm switch
#define INRC2_PERTRUB_IN_REBUILD

#define INRC2_BLOCK_SWAP_FIRST_IMPROVE

#define INRC2_BLOCK_SWAP_AVERAGE_TABU 0
#define INRC2_BLOCK_SWAP_STRONG_TABU 1
#define INRC2_BLOCK_SWAP_WEAK_TABU 2
#define INRC2_BLOCK_SWAP_NO_TABU 3
#define INRC2_BLOCK_SWAP_TABU_STRENGTH INRC2_BLOCK_SWAP_AVERAGE_TABU

#define INRC2_BLOCK_SWAP_ORGN 0
#define INRC2_BLOCK_SWAP_FAST 1
#define INRC2_BLOCK_SWAP_PART 2
#define INRC2_BLOCK_SWAP_RAND 3
#define INRC2_BLOCK_SWAP_FIND_BEST INRC2_BLOCK_SWAP_FAST


#endif