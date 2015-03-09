/**
*   usage : 1. just for test...
*
*   note :  1. use some bad tricks to reduce duplicated work.
*/

#ifndef INRC2_DEBUG_H
#define INRC2_DEBUG_H


#include "INRC2.h"


// all arguments are string literal
#define DIR "../INRC2/instance"
#define SCENARIO(instance) "Sc-" instance ".txt"
#define HISTORY(instance, sn) "H0-" instance "-" sn ".txt"
#define WEEKDATA(instance, sn) "WD-" instance "-" sn ".txt"


void test_n005w4_1233();


const char *fullArgv[] = {
    "INRC2.exe",
    "--sce", "",
    "--his", "",
    "--week", "",
    "--sol", "",
    "-cusIn", "",
    "--cusOut", "",
    "--rand", ""
    "--timeout", ""
};


#endif