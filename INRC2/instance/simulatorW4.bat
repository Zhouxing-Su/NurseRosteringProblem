%~d0
cd %~dp0
set instance=n005w4
set h0=0
set w1=1
set w2=2
set w3=3
set w4=3
set solverPath="../../Debug/INRC2.exe"
set outputDir=output
set timeout=1
java -jar Simulator.jar --his %instance%/H0-%instance%-%h0%.txt --sce %instance%/Sc-%instance%.txt --weeks %instance%/WD-%instance%-%w1%.txt %instance%/WD-%instance%-%w2%.txt %instance%/WD-%instance%-%w3%.txt %instance%/WD-%instance%-%w4%.txt --solver %solverPath% --outDir %outputDir% --timeout %timeout%
start log.csv