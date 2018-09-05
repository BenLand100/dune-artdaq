#! /bin/bash
# quick-mrb-start.sh - Eric Flumerfelt, May 20, 2016
# Downloads, installs, and runs the artdaq_demo as an MRB-controlled repository

# JCF, Jan-1-2017
# Modified this script to work with the lbne-artdaq package

# JCF, Mar-2-2017
# Modified it again to work with the brand new dune-artdaq package


if ! [[ "$HOSTNAME" =~ ^np04-srv ]]; then 
    echo "This script will only work on the CERN protoDUNE teststand computers, np04-srv-*" >&2
    exit 1
fi

if [[ "$HOSTNAME" == "np04-srv-001" || "$HOSTNAME" == "np04-srv-009" || "$HOSTNAME" == "np04-srv-010" ]]; then
    echo "Host $HOSTNAME doesn't physically have enough processors for a speedy build; try another host (e.g., np04-srv-014)"
    exit 1
fi

if [[ -e /nfs/sw/artdaq/products/setup ]]; then
    . /nfs/sw/artdaq/products/setup
else
    echo "Expected there to be a products directory /nfs/sw/artdaq/products/" >&2 
    exit 1
fi

bad_network=false


git_status=`git status 2>/dev/null`
git_sts=$?
if [ $git_sts -eq 0 ];then
    echo "This script is designed to be run in a fresh install directory!"
    exit 1
fi

starttime=`date`
Base=$PWD
test -d products || mkdir products
test -d download || mkdir download
test -d log || mkdir log

if $bad_network ; then
    echo "\"bad_network\" parameter is set; therefore quick-mrb-start.sh makes the following assumptions: "
    echo "-All needed products are on the host, and the setup for those products has been sourced"
    echo "-The git repos for artdaq, etc., are already in this directory"
    echo "-There's a file already existing called $Base/download/product_deps which will tell this script what version of artdaq, etc., to expect"
    echo "-Your host has the SLF7 operating system"
    echo "-You've deleted the directories which look like localProducts_dune_artdaq_v1_06_00_e10_prof and build_slf7.x86_64 (though the versions may have changed since this instruction was written)"
    sleep 5
fi



env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
   usage: `basename $0` [options]
examples: `basename $0` 
          `basename $0` --dune-raw-data-developer --dune-raw-data-develop-branch
          `basename $0` --debug --dune-raw-data-developer
--debug       perform a debug build
--not-dune-artdaq-developer  use if you don't have write access to the dune-artdaq repository
--dune-raw-data-develop-branch     Install the current \"develop\" version of dune-raw-data (may be unstable!)
--dune-raw-data-developer    use if you have (and want to use) write access to the dune-raw-data repository
"

# Process script arguments and options
eval env_opts=\${$env_opts_var-} # can be args too
eval "set -- $env_opts \"\$@\""
op1chr='rest=`expr "$op" : "[^-]\(.*\)"`   && set -- "-$rest" "$@"'
op1arg='rest=`expr "$op" : "[^-]\(.*\)"`   && set --  "$rest" "$@"'
reqarg="$op1arg;"'test -z "${1+1}" &&echo opt -$op requires arg. &&echo "$USAGE" &&exit'
args= do_help= opt_v=0; opt_lrd_w=0; opt_lrd_develop=0; opt_la_nw=0;
while [ -n "${1-}" ];do
    if expr "x${1-}" : 'x-' >/dev/null;then
        op=`expr "x$1" : 'x-\(.*\)'`; shift   # done with $1
        leq=`expr "x$op" : 'x-[^=]*\(=\)'` lev=`expr "x$op" : 'x-[^=]*=\(.*\)'`
        test -n "$leq"&&eval "set -- \"\$lev\" \"\$@\""&&op=`expr "x$op" : 'x\([^=]*\)'`
        case "$op" in
            \?*|h*)     eval $op1chr; do_help=1;;
	    -debug)     opt_debug=--debug;;
	    -not-dune-artdaq-developer) opt_la_nw=1;;
	    -dune-raw-data-develop-branch) opt_lrd_develop=1;;
	    -dune-raw-data-developer)  opt_lrd_w=1;;
            *)          echo "Unknown option -$op"; do_help=1;;
        esac
    else
        aa=`echo "$1" | sed -e"s/'/'\"'\"'/g"` args="$args '$aa'"; shift
    fi
done
eval "set -- $args \"\$@\""; unset args aa

which lsb_release 2>&1 > /dev/null
if [[ "$?" != "0" ]]; then
    echo "According to the \"which\" command, lsb_release is not available - this needs to be installed in order for dune-artdaq installation to work" >&2
    exit 1
fi


uhal_setup_cmd="setup uhal v2_6_0 -q e15:prof:s64"
$uhal_setup_cmd



set -u   # complain about uninitialed shell variables - helps development

test -n "${do_help-}" -o $# -ge 3 && echo "$USAGE" && exit

# JCF, 1/16/15
# Save all output from this script (stdout + stderr) in a file with a
# name that looks like "quick-start.sh_Fri_Jan_16_13:58:27.script" as
# well as all stderr in a file with a name that looks like
# "quick-start.sh_Fri_Jan_16_13:58:27_stderr.script"
alloutput_file=$( date | awk -v "SCRIPTNAME=$(basename $0)" '{print SCRIPTNAME"_"$1"_"$2"_"$3"_"$4".script"}' )
stderr_file=$( date | awk -v "SCRIPTNAME=$(basename $0)" '{print SCRIPTNAME"_"$1"_"$2"_"$3"_"$4"_stderr.script"}' )
exec  > >(tee "$Base/log/$alloutput_file")
exec 2> >(tee "$Base/log/$stderr_file")

function detectAndPull() {
    local startDir=$PWD
    cd $Base/download
    local packageName=$1
    local packageOs=$2

    if [ $# -gt 2 ];then
	local qualifiers=$3
    fi
    if [ $# -gt 3 ];then
	local packageVersion=$4
    else
	local packageVersion=`curl http://scisoft.fnal.gov/scisoft/packages/${packageName}/ 2>/dev/null|grep ${packageName}|grep "id=\"v"|tail -1|sed 's/.* id="\(v.*\)".*/\1/'`
    fi
    local packageDotVersion=`echo $packageVersion|sed 's/_/\./g'|sed 's/v//'`

    if [[ "$packageOs" != "noarch" ]]; then
        local upsflavor=`ups flavor`
	local packageQualifiers="-`echo $qualifiers|sed 's/:/-/g'`"
	local packageUPSString="-f $upsflavor -q$qualifiers"
    fi
    local packageInstalled=`ups list -aK+ $packageName $packageVersion ${packageUPSString-}|grep -c "$packageName"`
    if [ $packageInstalled -eq 0 ]; then
	local packagePath="$packageName/$packageVersion/$packageName-$packageDotVersion-${packageOs}${packageQualifiers-}.tar.bz2"
	echo wget http://scisoft.fnal.gov/scisoft/packages/$packagePath
	wget http://scisoft.fnal.gov/scisoft/packages/$packagePath >/dev/null 2>&1
	local packageFile=$( echo $packagePath | awk 'BEGIN { FS="/" } { print $NF }' )

	if [[ ! -e $packageFile ]]; then
	    echo "Unable to download $packageName"
	    exit 1
	fi

	local returndir=$PWD
	cd $Base/products
	tar -xjf $Base/download/$packageFile
	cd $returndir
    fi
    cd $startDir
}

if $bad_network ; then
    os="slf7"    
else 
    os=
fi

cd $Base/download

if [[ -z $os ]]; then
    echo "Cloning cetpkgsupport to determine current OS"
    git clone http://cdcvs.fnal.gov/projects/cetpkgsupport
    os=`./cetpkgsupport/bin/get-directory-name os`
fi

if [[ "$os" == "u14" ]]; then
	echo "-H Linux64bit+3.19-2.19" >../products/ups_OVERRIDE.`hostname`
fi

# Get all the information we'll need to decide which exact flavor of the software to install
if [ -z "${tag:-}" ]; then 
  tag=develop;
fi

if ! $bad_network; then
    wget https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/repository/revisions/develop/raw/ups/product_deps
fi

if [[ ! -e $Base/download/product_deps ]]; then
    echo "You need to have a product_deps file in $Base/download" >&2
    exit 1
fi

demo_version=`grep "parent dune_artdaq" $Base/download/product_deps|awk '{print $3}'`

artdaq_version=`grep -E "^artdaq\s+" $Base/download/product_deps | awk '{print $2}'`

if [[ "$artdaq_version" != "v3_00_03a" ]]; then
    artdaq_manifest_version=$artdaq_version
else
    artdaq_manifest_version=v3_00_03
fi


coredemo_version=`grep -E "^dune_raw_data\s+" $Base/download/product_deps | awk '{print $2}'`


equalifier=e15
squalifier=s64

if [[ -n "${opt_debug:-}" ]] ; then
    build_type="debug"
else
    build_type="prof"
fi


# If we aren't connected to the outside world, you'll need to have
# previously scp'd or rsync'd the products to the host you're trying
# to install dune-artdaq on

if ! $bad_network; then
    wget http://scisoft.fnal.gov/scisoft/bundles/tools/pullProducts
    chmod +x pullProducts
    ./pullProducts $Base/products ${os} artdaq-${artdaq_manifest_version} ${squalifier}-${equalifier} ${build_type}

    if [ $? -ne 0 ]; then
	echo "Error in pullProducts. Please go to http://scisoft.fnal.gov/scisoft/bundles/artdaq/${artdaq_manifest_version}/manifest and make sure that a manifest for the specified qualifiers (${squalifier}-${equalifier}) exists."
	exit 1
    fi

    detectAndPull mrb noarch
fi

# JCF, Apr-26-2018: Kurt discovered that it's necessary for the $Base/products area to be based on ups v6_0_7
upsfile=/nfs/home/np04daq/.jcfree/Documents/ups-6.0.7-Linux64bit+3.10-2.17.tar.bz2

if [[ ! -e $upsfile ]]; then
    echo "Unable to find ${upsfile}; you'll need to restore it from SciSoft" >&2
    exit 1
fi

cd $Base/products
cp -p $upsfile .
tar xjf $upsfile
source $Base/products/setup
setup mrb
setup git
setup gitflow

export MRB_PROJECT=dune_artdaq
cd $Base
mrb newDev -f -v $demo_version -q ${equalifier}:${build_type}
set +u





#setup netio
source $Base/localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}/setup
set -u

cd $MRB_SOURCE

if [[ $opt_lrd_develop -eq 1 ]]; then
    dune_raw_data_checkout_arg="-d dune_raw_data"
else
    dune_raw_data_checkout_arg="-b feature/artdaq_v3_02 -d dune_raw_data"
fi


if [[ $opt_lrd_w -eq 1 ]]; then
    dune_raw_data_repo="ssh://p-dune-raw-data@cdcvs.fnal.gov/cvs/projects/dune-raw-data"
else
    dune_raw_data_repo="http://cdcvs.fnal.gov/projects/dune-raw-data"
fi

if [[ $tag == "develop" ]]; then
    dune_artdaq_checkout_arg="-d dune_artdaq"
else
    dune_artdaq_checkout_arg="-b feature/artdaq_v3_02 -d dune_artdaq"
fi


# Notice the default for write access to dune-artdaq is the opposite of that for dune-raw-data
if [[ $opt_la_nw -eq 1 ]]; then
    dune_artdaq_repo="http://cdcvs.fnal.gov/projects/dune-artdaq"
else
    dune_artdaq_repo="ssh://p-dune-artdaq@cdcvs.fnal.gov/cvs/projects/dune-artdaq"
fi


if ! $bad_network; then

    mrb gitCheckout -b feature/pdune_ron_Aug01 -d artdaq_core http://cdcvs.fnal.gov/projects/artdaq-core
 
    if [[ "$?" != "0" ]]; then
        echo "Unable to perform checkout of http://cdcvs.fnal.gov/projects/artdaq-core"
        exit 1
    fi

    mrb gitCheckout -b feature/pdune_ron_Aug01 -d artdaq_utilities http://cdcvs.fnal.gov/projects/artdaq-utilities
 
    if [[ "$?" != "0" ]]; then
        echo "Unable to perform checkout of http://cdcvs.fnal.gov/projects/artdaq-utilities"
        exit 1
    fi


    mrb gitCheckout -b feature/pdune_ron_Aug01 -d artdaq http://cdcvs.fnal.gov/projects/artdaq

    if [[ "$?" != "0" ]]; then
    	echo "Unable to perform checkout of http://cdcvs.fnal.gov/projects/artdaq"
    	exit 1
    fi

    mrb gitCheckout -b feature/pdune_ron_Aug01 -d artdaq_mpich_plugin http://cdcvs.fnal.gov/projects/artdaq-utilities-mpich-plugin

   if [[ "$?" != "0" ]]; then
    	echo "Unable to perform checkout of http://cdcvs.fnal.gov/projects/artdaq-utilities-mpich_plugin"
    	exit 1
    fi

    mrb gitCheckout $dune_raw_data_checkout_arg $dune_raw_data_repo

    if [[ "$?" != "0" ]]; then
	echo "Unable to perform checkout of $dune_raw_data_repo"
	exit 1
    fi

    mrb gitCheckout $dune_artdaq_checkout_arg $dune_artdaq_repo

    if [[ "$?" != "0" ]]; then
	echo "Unable to perform checkout of $dune_artdaq_repo"
	exit 1
    fi

    # JCF, Aug-9-2018

    # Unclear why gitCheckout doesn't appear to simply *work* when I
    # try to get the dune-artdaq feature/artdaq_v3_02 branch, but
    # since it doesn't, I need to get it manually...

    cd dune_artdaq
    git fetch origin
    git checkout -b origin/feature/artdaq_v3_02
    git checkout feature/artdaq_v3_02
    cd ..
fi

sed -i -r 's/^\s*defaultqual(\s+).*/defaultqual\1'$equalifier':'$squalifier'/' artdaq/ups/product_deps
sed -i -r 's/^\s*defaultqual(\s+).*/defaultqual\1'$equalifier':online:'$squalifier'/' dune_raw_data/ups/product_deps


ARTDAQ_DEMO_DIR=$Base/srcs/dune_artdaq


nprocessors=$( grep -E "processor\s+:" /proc/cpuinfo | wc -l )
trace_file_label=$( basename $Base )

cd $Base
    cat >setupDUNEARTDAQ_forBuilding <<-EOF
       echo # This script is intended to be sourced.                                                                    
                                                                                                                         
        sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-artdaq.'; exit; }" || exit                                                                                           
        source /nfs/sw/artdaq/products/setup                                      

        setup mrb
        source $Base/localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}/setup
                                                                                                                  
        export DAQ_INDATA_PATH=$ARTDAQ_DEMO_DIR/test/Generators:$ARTDAQ_DEMO_DIR/inputData                               
                                                                                                                         
        export DUNEARTDAQ_BUILD=$MRB_BUILDDIR/dune_artdaq                                                            
        export DUNEARTDAQ_REPO="$ARTDAQ_DEMO_DIR"                                                                        
        export FHICL_FILE_PATH=.:\$DUNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH                                           

        setup protodune_wibsoft v341 -q e15:s64
        setup uhal v2_6_0 -q e15:prof:s64
        setup netio v0_8_0 -q e15:prof:s64
# RSIPOS 03/07/18 -> Infiniband workaround for FELIX BR build
        #source /nfs/sw/felix/HACK-FELIX-BR-BUILD.sh
        setup ftd2xx v1_2_7a
        setup dunepdsprce v0_0_4 -q e15:gen:prof
        setup rogue v2_10_0_3_gfaeedd0 -q e15:prof
        setup protodune_timing v4_1_3 -q e15:prof:s64
        setup dune_artdaq_InhibitMaster v0_01_01 -q e15:prof

        export DIM_INC=/nfs/sw/dim/dim_v20r20
        export DIM_LIB=/nfs/sw/dim/dim_v20r20/linux
        export LD_LIBRARY_PATH=\$DIM_LIB:\$LD_LIBRARY_PATH
        
setup jsoncpp v1_8_0 -q e15

        # 26-Apr-2018, KAB: moved the sourcing of mrbSetEnv to *after* all product
        # setups so that we get the right env vars for the source repositories that
        # are part of the mrb build area.
        source mrbSetEnv

echo "Only need to source this file once in the environment; now, to perform builds do the following:"
echo ""
echo "  mrb build -j32  # to build without 'installing' the built code"
echo "  mrb install -j32  # to build and 'install' the built code"
echo "    (where 'install' means copy-to-local-products-area and use in runtime environment)"
echo ""
echo "*** PLEASE NOTE that we should now use 'mrb install -j32' to build the dune-artdaq software ***"
echo ""

EOF


    cat >setupDUNEARTDAQ_forRunning <<-EOF
       echo # This script is intended to be sourced.                                                                    
                                                                                                                         
        sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-artdaq.'; exit; }" || exit                                                                                           
        source /nfs/sw/artdaq/products/setup                                      
        source $Base/localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}/setupWithoutMRB
        setup dune_artdaq v1_16_00 -q e15:prof
       
        alias RoutingMaster="routing_master"
                                                                                                           
        export DAQ_INDATA_PATH=$ARTDAQ_DEMO_DIR/test/Generators:$ARTDAQ_DEMO_DIR/inputData                               
                                                                                                                         
        export DUNEARTDAQ_BUILD=$MRB_BUILDDIR/dune_artdaq                                                            
        export DUNEARTDAQ_REPO="$ARTDAQ_DEMO_DIR"                                                                        
        export FHICL_FILE_PATH=.:\$DUNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH                                           

        setup protodune_wibsoft v341 -q e15:s64
        setup uhal v2_6_0 -q e15:prof:s64

        setup netio v0_8_0 -q e15:prof:s64
# RSIPOS 03/07/18 -> Infiniband workaround for FELIX BR build
#        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/felix/new-software/multidma/software/external/libfabric/1.4.1.3/x86_64-centos7-gcc62-opt/lib
        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/felix/fabric/build/lib
        #source /nfs/sw/felix/HACK-FELIX-BR-BUILD.sh
        setup ftd2xx v1_2_7a
        setup dunepdsprce v0_0_4 -q e15:gen:prof
        setup rogue v2_10_0_3_gfaeedd0 -q e15:prof
        setup protodune_timing v4_1_3 -q e15:prof:s64
        setup dune_artdaq_InhibitMaster v0_01_01 -q e15:prof
export PYTHONUSERBASE=/nfs/sw/work_dirs/dune-artdaq-InhibitMaster/user_python

        export DIM_INC=/nfs/sw/dim/dim_v20r20
        export DIM_LIB=/nfs/sw/dim/dim_v20r20/linux
        export LD_LIBRARY_PATH=\$DIM_LIB:\$LD_LIBRARY_PATH

       setup -j artdaq_dim_plugin v0_02_06 -q e15:prof:s64
        setup artdaq_mpich_plugin v1_00_02 -q e15:eth:prof:s64
        
       setup TRACE v3_13_07
        export TRACE_FILE=/tmp/trace_$trace_file_label
        export TRACE_LIMIT_MS="500,1000,2000"

setup jsoncpp v1_8_0 -q e15

     # Uncomment the "tonM" line for a given TRACE below which you
        # want to activate. If you don't see the TRACE you want, then
        # add a toffM / tonM combination for it like you see in the
        # other examples below. 

        #toffM 12 -n RequestSender
        #tonM 12 -n RequestSender
 
        #toffM 10 -n CommandableFragmentGenerator
        #tonM 10 -n CommandableFragmentGenerator

        toffM 50 -n RCE
        tonM  50 -n RCE

toffS 20 -n TriggerBoardReader
#tonS  20 -n TriggerBoardReader


# JCF, 11/25/14
# Make it easy for users to take a quick look at their output file via "rawEventDump"

alias rawEventDump="if [[ -n \\\$SETUP_TRACE ]]; then unsetup TRACE ; echo Disabling TRACE ; sleep 1; fi; art -c \$DUNEARTDAQ_REPO/tools/fcl/rawEventDump.fcl "                                                    

echo ""
echo "*** PLEASE NOTE that there are now TWO setup scripts:"
echo "    - setupDUNEARTDAQ_forBuilding"
echo "    - setupDUNEARTDAQ_forRunning"
echo ""
echo "You have just sourced setupDUNEARTDAQ_forRunning."
echo "Please use this script for all uses except building the software."
echo ""
echo "If, instead, you would like to build the software, please start with a fresh"
echo "shell and 'source setupDUNEARTDAQ_forBuilding' and 'mrb install -j 32'".
echo ""

# 02-Aug-2018, KAB: only enable core dumps for certain hosts
if [[ "\${HOSTNAME}" == "np04-srv-001" ]] ; then
    ulimit -c unlimited
fi
if [[ "\${HOSTNAME}" == "np04-srv-002" ]] ; then
    ulimit -c unlimited
fi
if [[ "\${HOSTNAME}" == "np04-srv-003" ]] ; then
    ulimit -c unlimited
fi
if [[ "\${HOSTNAME}" == "np04-srv-004" ]] ; then
    ulimit -c unlimited
fi

EOF

if [[ -e $Base/srcs/dune_artdaq/tools/setupWithoutMRB ]]; then
    cp $Base/srcs/dune_artdaq/tools/setupWithoutMRB $Base/localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}/setupWithoutMRB
else
    echo "Problem: there's no setupWithoutMRB script in $Base/srcs/dune_artdaq; exiting..." >&2
    exit 10
fi



cd $MRB_BUILDDIR

export CETPKG_J=$nprocessors
source /nfs/sw/artdaq/products/setup


        setup protodune_wibsoft v339 -q e15:s64
        setup uhal v2_6_0 -q e15:prof:s64
        setup netio v0_8_0 -q e15:prof:s64
# RSIPOS 03/07/18 -> Infiniband workaround for FELIX BR build
        #source /nfs/sw/felix/HACK-FELIX-BR-BUILD.sh
        setup ftd2xx v1_2_7a
        setup dunepdsprce v0_0_4 -q e15:gen:prof
        setup rogue v2_10_0_3_gfaeedd0 -q e15:prof
        setup protodune_timing v4_0_2 -q e15:prof:s64
        setup dune_artdaq_InhibitMaster v0_01_01 -q e15:prof
	setup jsoncpp v1_8_0 -q e15

set +u
source mrbSetEnv
set -u
mrb install -j$nprocessors                              

installStatus=$?

cd $Base
find build_slf7.x86_64/ -type d | xargs -i chmod g+rwx {}              
find build_slf7.x86_64/ -type f | xargs -i chmod g+rw {}               

if [ $installStatus -eq 0 ]; then
     echo "dune-artdaq has been installed correctly."
     echo
else
     echo "Build error. If all else fails, try (A) logging into a new terminal and (B) creating a new directory out of which to run this script."
     echo
fi

echo                                                               

endtime=`date`

echo "Build start time: $starttime"
echo "Build end time:   $endtime"

