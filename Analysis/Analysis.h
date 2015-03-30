/**
*   usage : 1. instance and solution analysis.
*
*   note :  1.
*/

#ifndef ANALYSIS_H
#define ANALYSIS_H


#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include "BatchTest.h"
#include "INRC2.h"
#include "NurseRostering.h"


void analyzeInstance();
void rebuildSolution( const std::string &filename, int lineNum );


#endif