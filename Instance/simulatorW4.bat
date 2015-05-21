%~d0
cd %~dp0
set instance=n030w4
set h0=1
set w1=6
set w2=2
set w3=9
set w4=1
set solverPath="../BatchRun/NurseRostering.exe"
set outputDir="output"
set timeout=70.13
set rand=1432202394
mkdir %outputDir%
java -jar Simulator_Old.jar --his %instance%/H0-%instance%-%h0%.txt --sce %instance%/Sc-%instance%.txt --weeks %instance%/WD-%instance%-%w1%.txt %instance%/WD-%instance%-%w2%.txt %instance%/WD-%instance%-%w3%.txt %instance%/WD-%instance%-%w4%.txt --solver %solverPath% --outDir %outputDir%/ --timeout %timeout% --rand %rand%
#start log.csv