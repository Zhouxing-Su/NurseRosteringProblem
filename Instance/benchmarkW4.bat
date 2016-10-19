%~d0
cd %~dp0
set instance=n070w4
set h0=0
set w1=3
set w2=6
set w3=5
set w4=1
set outputDir="output"
set timeout=1248.33
java -jar Simulator.jar --sce %instance%/Sc-%instance%.txt --his %instance%/H0-%instance%-%h0%.txt --weeks %instance%/WD-%instance%-%w1%.txt %instance%/WD-%instance%-%w2%.txt %instance%/WD-%instance%-%w3%.txt %instance%/WD-%instance%-%w4%.txt --solver ./NurseRostering.exe --outDir %outputDir%/ --timeout %timeout%
pause