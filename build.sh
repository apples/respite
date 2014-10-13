#!/bin/bash

CXX="g++"
CXXFLAGS="-O2 -std=c++11 -I/opt/local/include -I./tplibs"
LDFLAGS="-L/opt/local/lib"
LDLIBS="-lboost_system-mt -lboost_filesystem-mt"

$CXX $CXXFLAGS src/*.cpp tplibs/exec-stream.cpp $LDFLAGS $LDLIBS -o respite
