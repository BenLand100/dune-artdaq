#!/bin/bash

source `which setupDemoEnvironment.sh`

# create the configuration file for PMT
tempFile="/tmp/pmtConfig.$$"

echo "BoardReaderMain `hostname` ${DUNEARTDAQ_BR_PORT[0]}" >> $tempFile
echo "EventBuilderMain `hostname` ${DUNEARTDAQ_EB_PORT[0]}" >> $tempFile
echo "AggregatorMain `hostname` ${DUNEARTDAQ_AG_PORT[0]}" >> $tempFile

# create the logfile directories, if needed
logroot="/tmp"
mkdir -p -m 0777 ${logroot}/pmt
mkdir -p -m 0777 ${logroot}/masterControl
mkdir -p -m 0777 ${logroot}/boardreader
mkdir -p -m 0777 ${logroot}/eventbuilder
mkdir -p -m 0777 ${logroot}/aggregator

logfhicl=${DUNEARTDAQ_REPO}/tools/fcl/messagefacility.fcl

# start PMT
pmt.rb -p ${DUNEARTDAQ_PMT_PORT} -d $tempFile --logpath ${logroot} --logfhicl ${logfhicl} --display ${DISPLAY}
rm $tempFile
