#!/bin/bash

del log.csv
del err.csv
./INRC2_Simulator 2>> err.csv
./TOOL-SolutionAnalysis

platform=Linux
algorithm=GreedyInit+TSR+TSLARrCSEB
runDate=`date "+%Y%m%d%H%M"`
mv log.csv $runDate--$platform--$(algorithm).csv
mv analysisResult.csv $runDate--$platform--$algorithm--analysis.csv
mv checkResult.csv $runDate--$platform--$algorithm--check.csv
mv statistic.csv $runDate--$platform--$algorithm--statistic.csv