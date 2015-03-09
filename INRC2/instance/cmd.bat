%~d0
cd %~dp0
set instance=n005w4
java -jar Simulator.jar --his %instance%/H0-n005w4-0.txt --sce %instance%/Sc-n005w4.txt --weeks %instance%/WD-n005w4-2.txt %instance%/WD-n005w4-0.txt %instance%/WD-n005w4-2.txt %instance%/WD-n005w4-1.txt --solver ../../Debug/INRC2.exe --outDir output/
pause