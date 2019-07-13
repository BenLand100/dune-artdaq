#!/bin/bash
source /cvmfs/fermilab.opensciencegrid.org/products/artdaq/setup
PRODUCTS=$PRODUCTS:/cvmfs/fermilab.opensciencegrid.org/products/larsoft
setup czmq v4_2_0 -q e15
setup protobuf v3_3_1a -q e15


function testconnect {
    # arg: $1 name, $2 endpoint
    ../../../../../ptmp/build/test/check_recv --ntpsets 100 -a connect --socket-endpoints $2 --socket-pattern SUB --timeout-ms 1000 &>/dev/null
    if [ $? -eq 0 ]
    then
        echo "$1 is sending hits!"
    else
        echo "$1 is NOT sending hits!"
    fi
}


function dumphit {
    # args: $1 name, $2 endpoint, $3 time
    ../../../../../build/apps/czmqat isock --socket-pattern SUB --socket-attachment connect --socket-endpoints $2 ofile -f $1.dump &
    PID=$!
    # Wait for $3 seconds
    sleep $3
    # Kill it
    kill $PID
}

