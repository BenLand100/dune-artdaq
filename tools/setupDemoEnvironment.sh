# this script is intended to be sourced

###########################################
### fetch any user-specified parameters ###
###########################################
# function to print out usage hints
#scriptName=`basename $0`
scriptName="source setupDemoEnvironment.sh"
usage () {
    echo "
Usage: ${scriptName} [options]
Options:
  -h, --help: prints this usage message
  -p <base port number>: specifies the base port number
  -o <data directory>: specifies the directory for the output data files
Note: The base port number is used as the starting point for the ports that
      are assigned to the boardreader and eventbuilder processes, and it
      can be specified either by the command-line
      option or the LBNEARTDAQ_BASE_PORT environmental variable.
      It defaults to 5200 if neither are specified.
Note: The data directory defaults to /tmp if none is specified.
Example: ${scriptName} -p 5200
" >&2
}

# parse the command-line options
basePort=""
dataDir=""
OPTIND=1
while getopts "hp:o:-:" opt; do
    if [ "$opt" = "-" ]; then
        opt=$OPTARG
    fi
    case $opt in
        h | help)
            usage
            return 1
            ;;
        p)
            basePort=${OPTARG}
            ;;
        o)
            dataDir=${OPTARG}
            ;;
        *)
            usage
            return 1
            ;;
    esac
done
shift $(($OPTIND - 1))

# set a default base port, if needed
if [[ "${basePort}" == "" ]]; then
    if [[ "${LBNEARTDAQ_BASE_PORT}" != "" ]]; then
        basePort=${LBNEARTDAQ_BASE_PORT}
    else
        basePort=5300
    fi
fi

##################################
### set the necessary env vars ###
##################################
# port numbers
let LBNEARTDAQ_PMT_PORT=${basePort}
for idx in {0..19}
do
    let LBNEARTDAQ_BR_PORT[idx]=${basePort}+5+idx
    let LBNEARTDAQ_EB_PORT[idx]=${basePort}+35+idx
    let LBNEARTDAQ_AG_PORT[idx]=${basePort}+65+idx
done
#export LBNEARTDAQ_BR_PORT
#export LBNEARTDAQ_EB_PORT
#export LBNEARTDAQ_AG_PORT

# data directory
if [[ "${dataDir}" != "" ]]; then
    LBNEARTDAQ_DATA_DIR=$dataDir
else
    LBNEARTDAQ_DATA_DIR=/tmp
fi
export LBNEARTDAQ_DATA_DIR
