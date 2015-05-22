#!/bin/bash

echo -e `uname -o` "\n" \
`uname -r` "\n" \
`grep -m 1 'model name' /proc/cpuinfo` "\n" \
`grep -m 1 'MemTotal' /proc/meminfo` | tee platform.txt

./Instance/Benchmark_INRC2-Linux-x86-64 | sed '1d' \
| sed 's/nurses//g' | sed 's/ ---> //g' | sed 's/seconds//g'  \
| sed '1i 21 50' | sed '1i 12 30' | sed '1i 5 10' >timeout.txt
