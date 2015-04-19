del log.csv
del err.csv
INRC2_Simulator.exe 2>> err.csv
TOOL-ValidatorCheck.exe
TOOL-SolutionAnalysis.exe

set runDate=%date:~0,4%%date:~5,2%%date:~8,2%%time:~0,2%%time:~3,2%
set runDate=%runDate: =0%
rename log.csv %runDate%--Win7--GreedyInit+TSP+ACSR.csv
rename analysisResult.csv %runDate%--Win7--GreedyInit+TSP+ACSR--analysis.csv