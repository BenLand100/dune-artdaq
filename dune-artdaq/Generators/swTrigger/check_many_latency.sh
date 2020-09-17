#!/bin/bash

function run_one {
    connection="$1"
    outfile="$2"
    total_time="$3"
    check_latency --time "$total_time" --socket-attachment connect --socket-pattern SUB --socket-endpoints "${connection}" -f ${outfile} --timeout-ms 1000 &
    echo Started PID $! listening to \"${connection}\"
}

function run_many {
    N="$1"
    total_time="$2"
    machine="$3"
    firstport="$4"
    outbase="$5"
    outnum="$6"
    for i in `seq 0 $((N-1))`; do
        port=$((firstport + i))
        run_one "tcp://${machine}:${port}" "${outbase}$((outnum + i)).latency" "$total_time"
    done
}

function usage {
    cat <<EOF
     Usage:
        1.  $(basename $0) "n" "nsec" "ip" "firstport" "outbase" "outnum"
          
           or

        2.  $(basename $0) "config_file" "nsec"

     Case 1:
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

     Case 2:
     config_file:  a file containing lines like:

                   zeromq_connection    output_filename

                   one listener is created for each line, listening on zeromq_connection
                   and outputting to output_filename

     nsec:         number of seconds to run for
EOF
    exit 1
}

# FELIX BRs
# run_many 10 $NSEC 10.73.136.67 15141 felix 501
# run_many 3 $NSEC 10.73.136.60 15151 felix 601

if [ "$#" = 6 ]; then
    n="$1"
    nsec="$2"
    ip="$3"
    firstport="$4"
    outbase="$5"
    outnum="$6"

    run_many "$n" "$nsec" "$ip" "$firstport" "$outbase" "$outnum"

    wait
elif [ "$#" = 2 ]; then
    config_file="$1"
    nsec="$2"
    if [ ! -f "${config_file}" ]; then
        echo Can\'t read config file "${config_file}"
        exit 1
    fi
    while read connection outfile; do
        run_one "${connection}" "${outfile}" "${nsec}"
    done < "${config_file}"

    wait
else
    usage
fi


