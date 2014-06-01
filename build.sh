#!/bin/bash
g++ -g -static -static-libgcc -std=c++1y src/main.cpp -o jbuild -lboost_system -lboost_filesystem -lexecstream
