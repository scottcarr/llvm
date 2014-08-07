#!/bin/sh
unamestr=`uname`
if [[ "$unamestr" == 'Darwin' ]]; then
    ../../../Debug+Asserts/bin/opt test.ll -load ../../../Debug+Asserts/lib/ScottsPass.dylib -ScottsPass > /dev/null
else 
    ../../../Debug+Asserts/bin/opt test.ll -load ../../../Debug+Asserts/lib/ScottsPass.so -ScottsPass > /dev/null
fi

