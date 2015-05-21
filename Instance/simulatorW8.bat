%~d0
cd %~dp0
set instance=n120w8
set h0=0
set w1=9
set w2=2
set w3=3
set w4=5
set w5=8
set w6=6
set w7=3
set w8=1
set solverPath="../BatchRun/NurseRostering.exe"
set outputDir="output"
set timeout=10
set rand=1432202394
mkdir %outputDir%
java -jar Simulator_Old.jar --his %instance%/H0-%instance%-%h0%.txt --sce %instance%/Sc-%instance%.txt --weeks %instance%/WD-%instance%-%w1%.txt %instance%/WD-%instance%-%w2%.txt %instance%/WD-%instance%-%w3%.txt %instance%/WD-%instance%-%w4%.txt %instance%/WD-%instance%-%w5%.txt %instance%/WD-%instance%-%w6%.txt %instance%/WD-%instance%-%w7%.txt %instance%/WD-%instance%-%w8%.txt --solver %solverPath% --outDir %outputDir%/ --timeout %timeout% --rand %rand%
#start log.csv