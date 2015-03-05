#!/bin/bash

source `which setupDemoEnvironment.sh`

# create the configuration file for PMT
tempFile="/tmp/pmtConfig.$$"

echo "BoardReaderMain localhost ${LBNEARTDAQ_BR_PORT[0]}" >> $tempFile
echo "EventBuilderMain localhost ${LBNEARTDAQ_EB_PORT[0]}" >> $tempFile

# create the logfile directories, if needed
logroot="/data/lbnedaq/daqlogs"
mkdir -p -m 0777 ${logroot}/pmt
mkdir -p -m 0777 ${logroot}/masterControl
mkdir -p -m 0777 ${logroot}/boardreader
mkdir -p -m 0777 ${logroot}/eventbuilder
mkdir -p -m 0777 ${logroot}/aggregator

# start PMT
pmt.rb -p ${LBNEARTDAQ_PMT_PORT} -d $tempFile --logpath ${logroot} --display ${DISPLAY}
rm $tempFile
