%~d0
cd %~dp0
set instance=n120w4
set h0=1
set w1=4
set w2=6
set w3=2
set w4=6
set solverPath="../BatchRun/NurseRostering.exe"
set outputDir="output"
set timeout=529.22
set rand=1442324973
mkdir %outputDir%
java -jar Simulator_Old.jar --his %instance%/H0-%instance%-%h0%.txt --sce %instance%/Sc-%instance%.txt --weeks %instance%/WD-%instance%-%w1%.txt %instance%/WD-%instance%-%w2%.txt %instance%/WD-%instance%-%w3%.txt %instance%/WD-%instance%-%w4%.txt --solver %solverPath% --outDir %outputDir%/ --timeout %timeout% --rand %rand%
#start log.csv