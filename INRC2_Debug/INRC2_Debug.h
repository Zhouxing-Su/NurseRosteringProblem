/**
*   usage : 1. just for test...
*
*   note :  1. use some bad tricks to reduce duplicated work.
*/

#ifndef INRC2_DEBUG_H
#define INRC2_DEBUG_H


#include "INRC2.h"


// all arguments are string literal
#define DIR "instance"
#define SCENARIO(instance) "Sc-" instance ".txt"
#define WEEKDATA(instance, week) "WD-" instance "-" week ".txt"


void test_n005w4_1233();


const char *fullArgv[80] = {
    "INRC2.exe",
    "--sce", "",
    "--his", "",
    "--week", "",
    "--sol", "",
    "-cusIn", "",
    "--cusOut", "",
    "--rand", ""
};  // 15


#endif