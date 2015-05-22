#!/bin/bash

cp simulator $1

cd $1

rm log.csv
rm err.csv
./simulator
../analyzer

platform=Linux
algorithm=$1
runDate=`date "+%Y%m%d%H%M"`
mv log.csv ${runDate}--${platform}--${algorithm}.csv
mv analysisResult.csv ${runDate}--${platform}--${algorithm}--analysis.csv
mv checkResult.csv ${runDate}--${platform}--${algorithm}--check.csv
mv statistic.csv ${runDate}--${platform}--${algorithm}--statistic.csv

cd ..
