#!/bin/bash

source `which setupDemoEnvironment.sh`
THIS_NODE=`hostname -s`

# create the configuration file for PMT
tempFile="/tmp/pmtConfig.$$"

echo "BoardReaderMain ${THIS_NODE} ${DUNEARTDAQ_BR_PORT[0]}" >> $tempFile
echo "EventBuilderMain ${THIS_NODE} ${DUNEARTDAQ_EB_PORT[0]}" >> $tempFile
echo "EventBuilderMain ${THIS_NODE} ${DUNEARTDAQ_EB_PORT[1]}" >> $tempFile
echo "AggregatorMain ${THIS_NODE} ${DUNEARTDAQ_AG_PORT[0]}" >> $tempFile
echo "AggregatorMain ${THIS_NODE} ${DUNEARTDAQ_AG_PORT[1]}" >> $tempFile

# create the logfile directories, if needed
logroot="~/dune-artdaq/data"
mkdir -p -m 0777 ${logroot}/pmt
mkdir -p -m 0777 ${logroot}/masterControl
mkdir -p -m 0777 ${logroot}/boardreader
mkdir -p -m 0777 ${logroot}/eventbuilder
mkdir -p -m 0777 ${logroot}/aggregator

# start PMT
pmt.rb -p ${DUNEARTDAQ_PMT_PORT} -d $tempFile --logpath ${logroot} --display ${DISPLAY}
rm $tempFile
