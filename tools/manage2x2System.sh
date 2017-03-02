#!/bin/bash

source `which setupDemoEnvironment.sh` ""

AGGREGATOR_NODE=`hostname`
THIS_NODE=`hostname -s`

# this function expects a number of arguments:
#  1) the DAQ command to be sent
#  2) the run number (dummy for non-start commands)
#  3) whether to run online monitoring
#  4) the data directory
#  5) the logfile name
#  6) whether to write data to disk [0,1]
#  7) the desired size of each data file
#  8) the desired number of events in each file
#  9) the desired time duration of each file (minutes)
# 10) whether to print out CFG information (verbose)
function launch() {

  enableSerial=""
  if [[ "${10}" == "1" ]]; then
      enableSerial="-e"
  fi

  DemoControl.rb ${enableSerial} -s -c $1 \
    --v1720 `hostname`,${DUNEARTDAQ_BR_PORT[0]},0 \
    --v1720 `hostname`,${DUNEARTDAQ_BR_PORT[1]},1 \
    --eb `hostname`,${DUNEARTDAQ_EB_PORT[0]} \
    --eb `hostname`,${DUNEARTDAQ_EB_PORT[1]} \
    --data-dir ${4} --online-monitoring $3 \
    --write-data ${6} --file-size ${7} \
    --file-event-count ${8} --file-duration ${9} \
    --run-number $2 2>&1 | tee -a ${5}
}

# 30-Dec-2013, KAB - cut out from usage text below...
#  -s <file size>: specifies the size threshold for closing data files (in MB)
#      [default is 8000 MB (~7.8 GB); zero means that there is no file size limit]
#  --file-events <count>: specifies the desired number of events in each file
#      [default=0, which means no event count limit for files]
#  --file-duration <duration>: specifies the desired duration of each file (minutes)
#      [default=0, which means no duration limit for files]

scriptName=`basename $0`
usage () {
    echo "
Usage: ${scriptName} [options] <command>
Where command is one of:
  init, start, pause, resume, stop, status, get-legal-commands,
  shutdown, start-system, restart, reinit, exit,
  fast-shutdown, fast-restart, fast-reinit, or fast-exit
General options:
  -h, --help: prints this usage message
Configuration options (init commands):
  -m <on|off>: specifies whether to run online monitoring [default=off]
  -D : disables the writing of data to disk
  -o <data dir>: specifies the directory for data files [default=/tmp]
Begin-run options (start command):
  -N <run number>: specifies the run number
Notes:
  The start command expects a run number to be specified (-N).
  The primary commands are the following:
   * init - initializes (configures) the DAQ processes
   * start - starts a run
   * pause - pauses the run
   * resume - resumes the run
   * stop - stops the run
   * status - checks the status of each DAQ process
   * get-legal-commands - fetches the legal commands from each DAQ process
  Additional commands include:
   * shutdown - stops the run (if one is going), resets the DAQ processes
       to their ground state (if needed), and stops the MPI program (DAQ processes)
   * start-system - starts the MPI program (the DAQ processes)
   * restart - this is the same as a shutdown followed by a start-system
   * reinit - this is the same as a shutdown followed by a start-system and an init
   * exit - this resets the DAQ processes to their ground state, stops the MPI
       program, and exits PMT.
  Expert-level commands:
   * fast-shutdown - stops the MPI program (all DAQ processes) no matter what
       state they are in. This could have bad consequences if a run is going!
   * fast-restart - this is the same as a fast-shutdown followed by a start-system
   * fast-reinit - this is the same as a fast-shutdown followed by a start-system
       and an init
   * fast-exit - this stops the MPI program, and exits PMT.
Examples: ${scriptName} -p 32768 init
          ${scriptName} -N 101 start
" >&2
}

# parse the command-line options
originalCommand="$0 $*"
onmonEnable=off
diskWriting=1
dataDir="/tmp"
runNumber=""
fileSize=8000
fsChoiceSpecified=0
fileEventCount=0
fileDuration=0
verbose=0
OPTIND=1
while getopts "hc:N:o:t:m:Ds:w:v-:" opt; do
    if [ "$opt" = "-" ]; then
        opt=$OPTARG
    fi
    case $opt in
        h | help)
            usage
            exit 1
            ;;
	N)
            runNumber=${OPTARG}
            ;;
        m)
            onmonEnable=${OPTARG}
            ;;
        o)
            dataDir=${OPTARG}
            ;;
        D)
            diskWriting=0
            ;;
#        file-events)
#            fileEventCount=${!OPTIND}
#            let OPTIND=$OPTIND+1
#            ;;
#        file-duration)
#            fileDuration=${!OPTIND}
#            let OPTIND=$OPTIND+1
#            ;;
#        s)
#            fileSize=${OPTARG}
#            fsChoiceSpecified=1
#            ;;
        v)
            verbose=1
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
shift $(($OPTIND - 1))

# fetch the command to run
if [ $# -lt 1 ]; then
    usage
    exit
fi
command=$1
shift

# verify that the command is one that we expect
if [[ "$command" != "start-system" ]] && \
   [[ "$command" != "init" ]] && \
   [[ "$command" != "start" ]] && \
   [[ "$command" != "pause" ]] && \
   [[ "$command" != "resume" ]] && \
   [[ "$command" != "stop" ]] && \
   [[ "$command" != "status" ]] && \
   [[ "$command" != "get-legal-commands" ]] && \
   [[ "$command" != "shutdown" ]] && \
   [[ "$command" != "fast-shutdown" ]] && \
   [[ "$command" != "restart" ]] && \
   [[ "$command" != "fast-restart" ]] && \
   [[ "$command" != "reinit" ]] && \
   [[ "$command" != "fast-reinit" ]] && \
   [[ "$command" != "exit" ]] && \
   [[ "$command" != "fast-exit" ]]; then
    echo "Invalid command."
    usage
    exit
fi

# verify that the expected arguments were provided
if [[ "$command" == "start" ]] && [[ "$runNumber" == "" ]]; then
    echo ""
    echo "*** A run number needs to be specified."
    usage
    exit
fi

# fill in values for options that weren't specified
if [[ "$runNumber" == "" ]]; then
    runNumber=101
fi

# translate the onmon enable flag
if [[ "$onmonEnable" == "on" ]]; then
    onmonEnable=1
else
    onmonEnable=0
fi

# build the logfile name
TIMESTAMP=`date '+%Y%m%d%H%M%S'`
logFile="/tmp/masterControl/dsMC-${TIMESTAMP}-${command}.log"
echo "${originalCommand}" > $logFile
echo ">>> ${originalCommand} (Disk writing is ${diskWriting})"

# calculate the shmkey that should be checked
let shmKey=1078394880+${DUNEARTDAQ_PMT_PORT}
shmKeyString=`printf "0x%x" ${shmKey}`

# invoke the requested command
if [[ "$command" == "shutdown" ]]; then
    # first send a stop command to end the run (in case it is needed)
    launch "stop" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
    # next send a shutdown command to move the processes to their ground state
    launch "shutdown" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
    # stop the MPI program
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    # clean up any stale shared memory segment
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
elif [[ "$command" == "start-system" ]]; then
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.startSystem
elif [[ "$command" == "restart" ]]; then
    # first send a stop command to end the run (in case it is needed)
    launch "stop" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
    # next send a shutdown command to move the processes to their ground state
    launch "shutdown" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
    # stop the MPI program
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    # clean up any stale shared memory segment
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    # start the MPI program
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.startSystem
elif [[ "$command" == "reinit" ]]; then
    # first send a stop command to end the run (in case it is needed)
    launch "stop" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
    # next send a shutdown command to move the processes to their ground state
    launch "shutdown" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
    # stop the MPI program
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    # clean up any stale shared memory segment
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    # start the MPI program
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.startSystem
    # send the init command to re-initialize the system
    sleep 5
    launch "init" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
elif [[ "$command" == "exit" ]]; then
    launch "shutdown" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.exit
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"

elif [[ "$command" == "fast-shutdown" ]]; then
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
elif [[ "$command" == "fast-restart" ]]; then
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.startSystem
elif [[ "$command" == "fast-reinit" ]]; then
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.startSystem
    sleep 5
    launch "init" $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
elif [[ "$command" == "fast-exit" ]]; then
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.stopSystem
    xmlrpc ${THIS_NODE}:${DUNEARTDAQ_PMT_PORT}/RPC2 pmt.exit
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"

else
    launch $command $runNumber $onmonEnable $dataDir \
        $logFile $diskWriting $fileSize \
        $fileEventCount $fileDuration $verbose
fi
