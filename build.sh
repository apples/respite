#!/bin/bash

CXX="g++"
CPPFLAGS="-I./poco/include -DPOCO_STATIC"
CXXFLAGS="-O2 -std=c++11"
LDFLAGS="-static -L./poco/lib"
LDLIBS="-lboost_system -lboost_filesystem -lPocoFoundation"

mkdir obj
for f in `find src/ -name '*.cpp'`
do
    objname=obj/${f#src/}
    objname=${objname/.cpp/.o}
    if ! $CXX $CPPFLAGS $CXXFLAGS -c $f -o $objname
    then
        echo "BUILD FAILURE" >&2
        exit 1
    fi
done

if ! $CXX $CXXFLAGS $LDFLAGS `find obj/ -name '*.o'` $LDLIBS -o respite
then
    echo "LINK FAILURE" >&2
    exit 1
fi
