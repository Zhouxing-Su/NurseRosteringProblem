#!/bin/bash

g++ -std=c++11 -lpthread -o simulator -O2 simulator_src/*.cpp
g++ -std=c++11 -lpthread -o analyzer -O2 analyzer_src/*.cpp

