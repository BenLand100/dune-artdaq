#!/bin/bash

source `which setupDemoEnvironment.sh`

# create the configuration file for PMT
tempFile="/tmp/pmtConfig.$$"

echo "BoardReaderMain `hostname` ${LBNEARTDAQ_BR_PORT[0]}" >> $tempFile
echo "EventBuilderMain `hostname` ${LBNEARTDAQ_EB_PORT[0]}" >> $tempFile
echo "EventBuilderMain `hostname` ${LBNEARTDAQ_EB_PORT[1]}" >> $tempFile
echo "AggregatorMain `hostname` ${LBNEARTDAQ_AG_PORT[0]}" >> $tempFile
echo "AggregatorMain `hostname` ${LBNEARTDAQ_AG_PORT[1]}" >> $tempFile

# create the logfile directories, if needed
logroot="/tmp"
mkdir -p -m 0777 ${logroot}/pmt
mkdir -p -m 0777 ${logroot}/masterControl
mkdir -p -m 0777 ${logroot}/boardreader
mkdir -p -m 0777 ${logroot}/eventbuilder
mkdir -p -m 0777 ${logroot}/aggregator

# start PMT
pmt.rb -p ${LBNEARTDAQ_PMT_PORT} -d $tempFile --logpath ${logroot} --display ${DISPLAY}
rm $tempFile
