#!/bin/bash

#TESTDIR="."
TESTSCRIPT="check_latency"
FILESUF=".directFelix.mlt"
LOC="./logs6"

BOTHAPAS=0 #1=yes
APA=5 

FELIX=1 #1=yes
HSBR=0 #1=yes
TPWIN=1
TPSORT=1
TCBR=1
MLT=1
SINGLE=0

if [ ! -d "$LOC" ];then
  mkdir $LOC
  echo "Directory " $LOC " created."
else
  echo "Writing files to " $LOC
fi
echo "Setting up..."
source setup_while_running.sh

if [ -z "$1" ];
then
    NSEC=100
else
    NSEC=$1
fi

echo "Starting test" $TESTSCRIPT
echo "Writing files to " $LOC

declare -a PIDs

function ctrl_c() {
    echo "** Trapped CTRL-C, exiting all the processes"
    for i in "${PIDs[@]}"; do
        echo "$i";
        kill -15 $i
       echo "Process " $i " killed."	
    done
    ps
    ls -lh $LOC/*$FILESUF
}
trap ctrl_c INT

if [ "$BOTHAPAS" = "1" ];then
	APA=5
fi
if [ "$APA" = "5" ];then
	if [ "$FELIX" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15141" -f $LOC/felix501.latency$FILESUF &
		PIDs[ 1]=$!
		if [ "$SINGLE" != "1" ];then
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15142" -f $LOC/felix502.latency$FILESUF &
			PIDs[ 2]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15143" -f $LOC/felix503.latency$FILESUF &
			PIDs[ 3]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15144" -f $LOC/felix504.latency$FILESUF &
			PIDs[ 4]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15145" -f $LOC/felix505.latency$FILESUF &
			PIDs[ 5]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15146" -f $LOC/felix506.latency$FILESUF &
			PIDs[ 6]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15147" -f $LOC/felix507.latency$FILESUF &
			PIDs[ 7]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15148" -f $LOC/felix508.latency$FILESUF &
			PIDs[ 8]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15149" -f $LOC/felix509.latency$FILESUF &
			PIDs[ 9]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:15150" -f $LOC/felix510.latency$FILESUF &
			PIDs[10]=$!
		else
			HSBR=0
			TPWIN=0
			TPSORT=0
			TCBR=0
		fi
	fi
	if [ "$HSBR" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25141" -f $LOC/hsbr501.latency$FILESUF &
		PIDs[11]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25142" -f $LOC/hsbr502.latency$FILESUF &
		PIDs[12]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25143" -f $LOC/hsbr503.latency$FILESUF &
		PIDs[13]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25144" -f $LOC/hsbr504.latency$FILESUF &
		PIDs[14]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25145" -f $LOC/hsbr505.latency$FILESUF &
		PIDs[15]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25146" -f $LOC/hsbr506.latency$FILESUF &
		PIDs[16]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25147" -f $LOC/hsbr507.latency$FILESUF &
		PIDs[17]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25148" -f $LOC/hsbr508.latency$FILESUF &
		PIDs[18]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25149" -f $LOC/hsbr509.latency$FILESUF &
		PIDs[19]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.67:25150" -f $LOC/hsbr510.latency$FILESUF &
		PIDs[20]=$!
	fi
	if [ "$TPWIN" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30141" -f $LOC/tpwin501.latency$FILESUF &
		PIDs[21]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30142" -f $LOC/tpwin502.latency$FILESUF &
		PIDs[22]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30143" -f $LOC/tpwin503.latency$FILESUF &
		PIDs[23]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30144" -f $LOC/tpwin504.latency$FILESUF &
		PIDs[24]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30145" -f $LOC/tpwin505.latency$FILESUF &
		PIDs[25]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30146" -f $LOC/tpwin506.latency$FILESUF &
		PIDs[26]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30147" -f $LOC/tpwin507.latency$FILESUF &
		PIDs[27]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30148" -f $LOC/tpwin508.latency$FILESUF &
		PIDs[28]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30149" -f $LOC/tpwin509.latency$FILESUF &
		PIDs[29]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30150" -f $LOC/tpwin510.latency$FILESUF &
		PIDs[30]=$!
	fi
	if [ "$TPSORT" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:35501" -f $LOC/tpsort500.latency$FILESUF &
		PIDs[31]=$!
	fi
	if [ "$TCBR" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:40501" -f $LOC/tcbr500.latency$FILESUF &
		PIDs[32]=$!
	fi
fi
if [ "$BOTHAPAS" = "1" ];then
	APA=6
fi
if [ "$APA" = "6" ];then
	if [ "$FELIX" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15151" -f $LOC/felix601.latency$FILESUF &
		PIDs[41]=$!
		if [ "$SINGLE" != "1" ];then
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15152" -f $LOC/felix602.latency$FILESUF &
			PIDs[42]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15153" -f $LOC/felix603.latency$FILESUF &
			PIDs[43]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15154" -f $LOC/felix604.latency$FILESUF &
			PIDs[44]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15155" -f $LOC/felix605.latency$FILESUF &
			PIDs[45]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15156" -f $LOC/felix606.latency$FILESUF &
			PIDs[46]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15157" -f $LOC/felix607.latency$FILESUF &
			PIDs[47]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15158" -f $LOC/felix608.latency$FILESUF &
			PIDs[48]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15159" -f $LOC/felix609.latency$FILESUF &
			PIDs[49]=$!
			$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:15160" -f $LOC/felix610.latency$FILESUF &
			PIDs[50]=$!
		else
			HSBR=0
			TPWIN=0
			TPSORT=0
			TCBR=0
		fi
	fi
	if [ "$HSBR" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25151" -f $LOC/hsbr601.latency$FILESUF &
		PIDs[51]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25152" -f $LOC/hsbr602.latency$FILESUF &
		PIDs[52]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25153" -f $LOC/hsbr603.latency$FILESUF &
		PIDs[53]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25154" -f $LOC/hsbr604.latency$FILESUF &
		PIDs[54]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25155" -f $LOC/hsbr605.latency$FILESUF &
		PIDs[55]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25156" -f $LOC/hsbr606.latency$FILESUF &
		PIDs[56]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25157" -f $LOC/hsbr607.latency$FILESUF &
		PIDs[57]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25158" -f $LOC/hsbr608.latency$FILESUF &
		PIDs[58]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25159" -f $LOC/hsbr609.latency$FILESUF &
		PIDs[59]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.60:25160" -f $LOC/hsbr610.latency$FILESUF &
		PIDs[60]=$!
	fi
	if [ "$TPWIN" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30151" -f $LOC/tpwin601.latency$FILESUF &
		PIDs[61]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30152" -f $LOC/tpwin602.latency$FILESUF &
		PIDs[62]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30153" -f $LOC/tpwin603.latency$FILESUF &
		PIDs[63]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30154" -f $LOC/tpwin604.latency$FILESUF &
		PIDs[64]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30155" -f $LOC/tpwin605.latency$FILESUF &
		PIDs[65]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30156" -f $LOC/tpwin606.latency$FILESUF &
		PIDs[66]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30157" -f $LOC/tpwin607.latency$FILESUF &
		PIDs[67]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30158" -f $LOC/tpwin608.latency$FILESUF &
		PIDs[68]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30159" -f $LOC/tpwin609.latency$FILESUF &
		PIDs[69]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:30160" -f $LOC/tpwin610.latency$FILESUF &
		PIDs[70]=$!
	fi
	if [ "$TPSORT" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:35601" -f $LOC/tpsort600.latency$FILESUF &
		PIDs[71]=$!
	fi
	if [ "$TCBR" = "1" ];then
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:40601" -f $LOC/tcbr600.latency$FILESUF &
		PIDs[72]=$!
	fi
fi
echo $MLT
if [ "$SINGLE" != "1" ];then
	if [ "$MLT" = "1" ];then
#		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:50501" -f $LOC/mlt501.latency$FILESUF &
#		PIDs[80]=$!
		$TESTSCRIPT  --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://10.73.136.32:50502" -f $LOC/mlt502.latency$FILESUF &
		PIDs[81]=$!
	fi
fi
sleep $NSEC

echo "Terminating..."
for i in "${PIDs[@]}"; do
    echo "$i";
    kill -15 $i
    echo "Process " $i " killed."	
done
ps
ls -lh $LOC/*$FILESUF
echo "Terminated."
