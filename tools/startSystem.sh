#!/bin/bash

if [ $# != 2 ] ; then
    echo "Usage: $0 <# of BoardReaderMains> <# of EventBuilderMains>"
    exit 1
fi

nBoardReaders=$1
nEventBuilders=$2
nAggregators=2  # Currently can have no more than 2 AggregatorMain processes

source `which setupDemoEnvironment.sh`

# create the configuration file for PMT
tempFile="/tmp/pmtConfig.$$"

for ((i=0; i<$nBoardReaders; i++)) ; do
    echo "BoardReaderMain `hostname` ${DUNEARTDAQ_BR_PORT[$i]}" >> $tempFile    
done

for ((i=0; i<$nEventBuilders; i++)) ; do
    echo "EventBuilderMain `hostname` ${DUNEARTDAQ_EB_PORT[$i]}" >> $tempFile    
done

for ((i=0; i<$nAggregators; i++)) ; do
    echo "AggregatorMain `hostname` ${DUNEARTDAQ_AG_PORT[$i]}" >> $tempFile
done

# create the logfile directories, if needed
logroot="/tmp"

for logdir in pmt masterControl boardreader eventbuilder aggregator ; do
    mkdir -p -m 0777 ${logroot}/$logdir
done

# start PMT
pmt.rb -p ${DUNEARTDAQ_PMT_PORT} -d $tempFile --logpath ${logroot} --display ${DISPLAY}
rm $tempFile
