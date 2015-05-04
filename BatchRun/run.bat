del log.csv
del err.csv
INRC2_Simulator.exe 2>> err.csv
TOOL-ValidatorCheck.exe
TOOL-SolutionAnalysis.exe
TOOL-CheckAnalysis.exe

set platform=Win8.1
set algorithm=GreedyInit+TSR+ACBR
set runDate=%date:~0,4%%date:~5,2%%date:~8,2%%time:~0,2%%time:~3,2%
set runDate=%runDate: =0%
rename log.csv %runDate%--%platform%--%algorithm%.csv
rename analysisResult.csv %runDate%--%platform%--%algorithm%--analysis.csv
rename checkResult.csv %runDate%--%platform%--%algorithm%--check.csv
rename statistic.csv %runDate%--%platform%--%algorithm%--statistic.csv