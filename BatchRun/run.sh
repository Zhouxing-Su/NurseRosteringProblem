#!/bin/bash

del log.csv
del err.csv
./INRC2_Simulator 2>> err.csv
./TOOL-SolutionAnalysis

platform=Linux
algorithm=GreedyInit+TSR+TSLARrCSEB
runDate=`date "+%Y%m%d%H%M"`
rename log.csv $runDate--$platform--$(algorithm).csv
rename analysisResult.csv $runDate--$platform--$algorithm--analysis.csv
rename checkResult.csv $runDate--$platform--$algorithm--check.csv

