del log.csv
del err.csv
INRC2_Simulator.exe 2>> err.csv
TOOL-ValidatorCheck.exe
TOOL-SolutionAnalysis.exe

set runDate=%date:~0,4%%date:~5,2%%date:~8,2%
rename log.csv %runDate%--Win7--GreedyInit+Tabu+ARRCS.csv
rename analysisResult.csv %runDate%--Win7--GreedyInit+Tabu+ARRCS--analysis.csv