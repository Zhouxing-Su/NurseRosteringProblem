%~d0
cd %~dp0
set instance=n005w4
set h0=2
set w1=4
set w2=2
set w3=0
set w4=3
set solverPath="../../Release/NurseRostering.exe"
set outputDir="output"
set timeout=10
set rand=10
mkdir %outputDir%
java -jar Simulator.jar --his %instance%/H0-%instance%-%h0%.txt --sce %instance%/Sc-%instance%.txt --weeks %instance%/WD-%instance%-%w1%.txt %instance%/WD-%instance%-%w2%.txt %instance%/WD-%instance%-%w3%.txt %instance%/WD-%instance%-%w4%.txt --solver %solverPath% --outDir %outputDir%/ --timeout %timeout% --rand %rand%
start log.csv