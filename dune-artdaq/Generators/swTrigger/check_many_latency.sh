#!/bin/bash

function run_one(){
    machine="$1"
    port="$2"
    outfile="$3"
    total_time="$4"
    ./check_latency --time "$total_time" --socket-attachment connect --socket-pattern SUB --socket-endpoints "tcp://${machine}:${port}" -f ${outfile} &
    echo Started PID $! listening to "tcp://${machine}:${port}"
}

function run_many() {
    N="$1"
    total_time="$2"
    machine="$3"
    firstport="$4"
    outbase="$5"
    outnum="$6"
    for i in `seq 0 $((N-1))`; do
        port=$((firstport + i))
        run_one "${machine}" "${port}" "${outbase}$((outnum + i)).latency" "$total_time"
    done
}

# FELIX BRs
# run_many 10 $NSEC 10.73.136.67 15141 felix 501
# run_many 3 $NSEC 10.73.136.60 15151 felix 601

# Hit-sending BRs
# run_many 10 $NSEC 10.73.136.67 25141 hitsend 501
# run_many 3 $NSEC 10.73.136.60 25151 hitsend 601

if [ "$#" != 6 ]; then
    cat <<EOF
     Usage: $(basename $0) "n" "nsec" "ip" "firstport" "outbase" "outnum"
    
     n:         number of links to subscribe to
     nsec:      number of seconds to run for
     ip:        ip address of machine to connect to
     firstport: the port number of the first port to connect to.
                A total of n connections are made, to firstport, firstport+1, ... firstport+n-1
     outbase:   the name (including path) of the output file(s)
     outnum:    the number appended to the first output file

     Full output file names are ${outbase}${outnum}.latency, ${outbase}${outnum}+1.latency etc
     
     Eg to connect to 3 APA 5 FELIX BRs for 100 seconds:

     $(basename $0) 3 100 10.73.136.67 15141 /my/dir/felix 501

     will connect to ports 15141, 15142, 15143 on 10.73.136.67
     and produce output files: /my/dir/felix501.latency
                               /my/dir/felix502.latency
                               /my/dir/felix503.latency
EOF
    exit 1
fi

n="$1"
nsec="$2"
ip="$3"
firstport="$4"
outbase="$5"
outnum="$6"

run_many "$n" "$nsec" "$ip" "$firstport" "$outbase" "$outnum"

wait


