#!/bin/bash

qsub -wd $1 ./run.sh %1
