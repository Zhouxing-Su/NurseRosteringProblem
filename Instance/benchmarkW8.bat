%~d0
cd %~dp0
set instance=n070w8
set h0=0
set w1=3
set w2=3
set w3=9
set w4=2
set w5=3
set w6=7
set w7=5
set w8=2
set outputDir="output"
set timeout=312.33
mkdir %outputDir%
java -jar Simulator.jar --sce %instance%/Sc-%instance%.txt --his %instance%/H0-%instance%-%h0%.txt --weeks %instance%/WD-%instance%-%w1%.txt %instance%/WD-%instance%-%w2%.txt %instance%/WD-%instance%-%w3%.txt %instance%/WD-%instance%-%w4%.txt %instance%/WD-%instance%-%w5%.txt %instance%/WD-%instance%-%w6%.txt %instance%/WD-%instance%-%w7%.txt %instance%/WD-%instance%-%w8%.txt --solver ./NurseRostering.exe --outDir %outputDir%/ --timeout %timeout%
pause