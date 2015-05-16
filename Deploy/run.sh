#!/bin/bash

echo -e `uname -o` "\n" \
`uname -r` "\n" \
`grep -m 1 'model name' /proc/cpuinfo` "\n" \
`grep -m 1 'MemTotal' /proc/meminfo` | tee platform.txt

../Instance/Benchmark_INRC2-Linux-x86-64 | tee timeout.txt \
| sed -i '1d' |sed -i 's/nurses//g' | sed -i 's/--->//g' | sed -i 's/seconds//g' \
| sed -i '1a 21 50' | sed -i '1a 12 30' | sed -i '1a 5 10'

rm log.csv
rm err.csv
./simulator
../analyzer

platform=Linux
algorithm=GreedyInit+$1
runDate=`date "+%Y%m%d%H%M"`
mv log.csv $runDate--$platform--$(algorithm).csv
mv analysisResult.csv $runDate--$platform--$algorithm--analysis.csv
mv checkResult.csv $runDate--$platform--$algorithm--check.csv
mv statistic.csv $runDate--$platform--$algorithm--statistic.csv
