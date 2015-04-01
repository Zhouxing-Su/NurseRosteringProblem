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
#include <cstring>
#include <cstdlib>

#include "BatchTest.h"
#include "INRC2.h"
#include "NurseRostering.h"


enum MaxLen
{
    // 120 nurses * 7 days a week * 2 numbers to record * 3 digits a number * other data
    LINE = 120 * 7 * 2 * 3 * 2,
    INST_NAME = 10,
    INIT_HIS_NAME = 100,
    WEEKDATA_NAME = 100,
    ALGORITHM_NAME = 100
};

struct ValidatorArgvPack
{
public:
    std::string instName;
    std::string sceName;
    std::string initHisName;
    std::vector<std::string> weekdataName;
    std::vector<std::string> solFileName;
};


extern const std::string validatorResultName;
extern const std::string H1Name;
extern const std::string H2name;
extern const std::string H3name;
extern const std::string H4name;
extern const std::string totalObjName;


void analyzeInstance();

// if no such record or record is incomplete, return -1, else totalObj.
// if return totalObj, the ValidatorArgvPack is filled, and a serial of 
// files named [week]sol.txt have been created in output directory
NurseRostering::ObjValue rebuildSolution( const std::string &logFileName, const std::string &logTime, const std::string id, ValidatorArgvPack &argv );

// call validator to generate validator result
int callValidator(const ValidatorArgvPack &vap);

// check all complete record in log file by calling validator
void validatorCheck( const std::string &logFileName, const std::string &outputFileName );
// return total objective value, -1 for infeasible
NurseRostering::ObjValue getObjValueInValidatorResult();


#endif