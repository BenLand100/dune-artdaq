#! /bin/bash
# quick-mrb-start.sh - Eric Flumerfelt, May 20, 2016
# Downloads, installs, and runs the artdaq_demo as an MRB-controlled repository

# JCF, Jan-1-2017
# Modified this script to work with the lbne-artdaq package

# JCF, Mar-2-2017
# Modified it again to work with the brand new dune-artdaq package

if [[ "$USER" == "np04daq" ]]; then
    
    cat<<EOF >&2

    Unless the dune-artdaq installation you're creating is meant for
    standard experiment-wide running rather than for code development
    purposes, you should run this script under your own user account,
    not under the np04daq account

EOF

exit 1

fi

if ! [[ "$HOSTNAME" =~ ^np04-srv ]]; then 
    echo "This script will only work on the CERN protoDUNE teststand computers, np04-srv-*" >&2
    exit 1
else
    :  # Note to myself (JCF): should consider putting a cvmfs source in here if I formalize this script for non-ProtoDUNE cluster installation
fi

if [[ -e /nfs/sw/artdaq/products/setup ]]; then
    . /nfs/sw/artdaq/products/setup
else
    echo "Expected there to be a products directory /nfs/sw/artdaq/products/" >&2 
    exit 1
fi

if [[ -e /nfs/sw/artdaq/products_dev/setup ]]; then
    . /nfs/sw/artdaq/products_dev/setup
else
    echo "Expected there to be a products directory /nfs/sw/artdaq/products_dev/" >&2
    exit 1
fi

Base=$PWD

if [[ -n $( find . -maxdepth 1 -not -name "quick-mrb-start*" -not -name ".") ]]; then

    cat<<EOF >&2

There appear to be files and/or subdirectories in this directory
besides this script; this script should only be run in a clean
directory. Exiting...

EOF

    exit 1
    
fi

if [[ -z $USER ]]; then

echo "Error: this script only works if you've got the \$USER environment variable set and pointing to your username. Please set this." >&2

    exit 1
fi

qms_tmpdir=/tmp/${USER}_for_quick-mrb-start
mkdir -p $qms_tmpdir
    
if [[ "$?" != "0" ]]; then
    echo "There was a problem running mkdir -p $qms_tmpdir; exiting..." >&2
    exit 1
fi

returndir=$PWD
cd $qms_tmpdir
rm -f quick-mrb-start.sh
wget https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/repository/revisions/develop/raw/tools/quick-mrb-start.sh

quick_mrb_start_edits=$( diff $Base/quick-mrb-start.sh $qms_tmpdir/quick-mrb-start.sh ) 
cd $returndir

if [[ -n $quick_mrb_start_edits ]]; then
    
    cat<<EOF >&2

Error: this script you're trying to run doesn't match with the version
of the script at the head of the develop branch in the dune-artdaq's
central repository. This may mean that this script makes obsolete
assumptions, etc., which could compromise your working
environment. Please delete this script and install your dune-artdaq
area according to the instructions at
https://twiki.cern.ch/twiki/bin/view/CENF/Installation

EOF

    exit 1
fi

starttime=`date`
test -d products || mkdir products
test -d download || mkdir download
test -d log || mkdir log

dune_artdaq_branch="develop"
dune_raw_data_branch="for_dune-artdaq"

env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
   usage: `basename $0` [options]
examples: `basename $0` 
          `basename $0` --dune-raw-data-developer 
          `basename $0` --debug --dune-raw-data-developer
--debug       perform a debug build
--include-artdaq-repos  use if you plan on developing the artdaq code (contact jcfree@fnal.gov or biery@fnal.gov before attempting this)
--use-dune-artdaq-branch <branchname>  instead of checking out the $dune_artdaq_branch branch of dune-artdaq, check out <branchname>
--use-dune-raw-data-branch <branchname>  instead of checking out the $dune_raw_data_branch branch of dune-raw-data, check out <branchname>
--not-dune-artdaq-developer  use if you don't have write access to the dune-artdaq repository
--dune-raw-data-developer    use if you have (and want to use) write access to the dune-raw-data repository
"

# Process script arguments and options
eval env_opts=\${$env_opts_var-} # can be args too
eval "set -- $env_opts \"\$@\""
op1chr='rest=`expr "$op" : "[^-]\(.*\)"`   && set -- "-$rest" "$@"'
op1arg='rest=`expr "$op" : "[^-]\(.*\)"`   && set --  "$rest" "$@"'
reqarg="$op1arg;"'test -z "${1+1}" &&echo opt -$op requires arg. &&echo "$USAGE" &&exit'
args= do_help= opt_v=0; opt_lrd_w=0; opt_la_nw=0; inc_artdaq_repos=0; 


while [ -n "${1-}" ];do
    if expr "x${1-}" : 'x-' >/dev/null;then
        op=`expr "x$1" : 'x-\(.*\)'`; shift   # done with $1
        leq=`expr "x$op" : 'x-[^=]*\(=\)'` lev=`expr "x$op" : 'x-[^=]*=\(.*\)'`
        test -n "$leq"&&eval "set -- \"\$lev\" \"\$@\""&&op=`expr "x$op" : 'x\([^=]*\)'`
        case "$op" in
            \?*|h*)     eval $op1chr; do_help=1;;
	    -debug)     opt_debug=--debug;;
	    -include-artdaq-repos) inc_artdaq_repos=1;;
	    -use-dune-artdaq-branch) eval $reqarg; dune_artdaq_branch=$1; shift;;
	    -use-dune-raw-data-branch) eval $reqarg; dune_raw_data_branch=$1; shift;;
	    -not-dune-artdaq-developer) opt_la_nw=1;;
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

cd $Base/download

# Get all the information we'll need to decide which exact flavor of the software to install
if [ -z "${tag:-}" ]; then 
  tag=develop;
fi

returndir=$PWD
cd $qms_tmpdir
rm -f product_deps
wget -O product_deps https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/repository/raw/ups/product_deps?rev=$( echo $dune_artdaq_branch | sed -r 's!/!%2F!g' )

if [[ "$?" != "0" ]]; then
    echo "Problem trying to get product_deps file for branch $dune_artdaq_branch off the web; exiting..." >&2
    exit 1
fi

demo_version=$( grep "parent dune_artdaq" ./product_deps|awk '{print $3}' )
artdaq_version=$( grep -E "^artdaq\s+" ./product_deps | awk '{print $2}' )

cd $returndir


if [[ "$artdaq_version" == "v3_00_03a" ]]; then
    artdaq_manifest_version=v3_00_03
elif [[ "$artdaq_version" == "v3_03_00_beta" ]]; then
    artdaq_manifest_version=v3_03_00
else
    artdaq_manifest_version=$artdaq_version
fi


equalifier=e15
squalifier=s64

if [[ -n "${opt_debug:-}" ]] ; then
    build_type="debug"
else
    build_type="prof"
fi

echo >&2
echo "Skipping pullProducts, products needed for artdaq already assumed to be available. If this is wrong, then from $PWD, execute:" >&2
echo "wget http://scisoft.fnal.gov/scisoft/bundles/tools/pullProducts" >&2
echo "chmod +x pullProducts" >&2
echo "./pullProducts $Base/products <OS name - e.g., slf7> artdaq-${artdaq_manifest_version} ${squalifier}-${equalifier} ${build_type}" >&2
echo >&2

detectAndPull mrb noarch

# JCF, Apr-26-2018: Kurt discovered that it's necessary for the $Base/products area to be based on ups v6_0_7
upsfile=/nfs/sw/control_files/misc/ups-6.0.7-Linux64bit+3.10-2.17.tar.bz2

if [[ ! -e $upsfile ]]; then
    echo "Unable to find ${upsfile}; you'll need to restore it from SciSoft" >&2
    exit 1
fi

cd $Base/products
cp -p $upsfile .
tar xjf $upsfile
source $Base/products/setup
setup mrb v1_14_00
setup git
setup gitflow

export MRB_PROJECT=dune_artdaq
export localproducts_subdir=localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}
cd $Base
mrb newDev -f -v $demo_version -q ${equalifier}:${build_type}
set +u

source $Base/$localproducts_subdir/setup
set -u

cd $MRB_SOURCE

dune_raw_data_checkout_arg="-b $dune_raw_data_branch -d dune_raw_data"

if [[ $opt_lrd_w -eq 1 ]]; then
    dune_raw_data_repo="ssh://p-dune-raw-data@cdcvs.fnal.gov/cvs/projects/dune-raw-data"
else
    dune_raw_data_repo="https://cdcvs.fnal.gov/projects/dune-raw-data"
fi

dune_artdaq_checkout_arg="-b $dune_artdaq_branch -d dune_artdaq"

# Notice the default for write access to dune-artdaq is the opposite of that for dune-raw-data
if [[ $opt_la_nw -eq 1 ]]; then
    dune_artdaq_repo="https://cdcvs.fnal.gov/projects/dune-artdaq"
else
    dune_artdaq_repo="ssh://p-dune-artdaq@cdcvs.fnal.gov/cvs/projects/dune-artdaq"
fi


if [[ "$inc_artdaq_repos" == "1" ]] ; then

mrb gitCheckout -b for_dune-artdaq -d artdaq_core https://cdcvs.fnal.gov/projects/artdaq-core
 
if [[ "$?" != "0" ]]; then
    echo "Unable to perform checkout of https://cdcvs.fnal.gov/projects/artdaq-core"
    exit 1
fi

mrb gitCheckout -b for_dune-artdaq -d artdaq_utilities https://cdcvs.fnal.gov/projects/artdaq-utilities
 
if [[ "$?" != "0" ]]; then
    echo "Unable to perform checkout of https://cdcvs.fnal.gov/projects/artdaq-utilities"
    exit 1
fi

mrb gitCheckout -b for_dune-artdaq -d artdaq https://cdcvs.fnal.gov/projects/artdaq

if [[ "$?" != "0" ]]; then
    echo "Unable to perform checkout of https://cdcvs.fnal.gov/projects/artdaq"
    exit 1
fi

sed -i -r 's/^\s*defaultqual(\s+).*/defaultqual\1'$equalifier':'$squalifier'/' artdaq/ups/product_deps

fi  # End "include artdaq repos"

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

sed -i -r 's/^\s*defaultqual(\s+).*/defaultqual\1'$equalifier':online:'$squalifier'/' dune_raw_data/ups/product_deps
dune_artdaq_version=$( sed -r -n 's/^\s*parent\s+\S+\s+(\S+).*/\1/p' dune_artdaq/ups/product_deps )

ARTDAQ_DEMO_DIR=$Base/srcs/dune_artdaq


nprocessors=$( grep -E "processor\s+:" /proc/cpuinfo | wc -l )
trace_file_label=$( basename $Base )

dune_artdaq_InhibitMaster_version=$( sed -r -n "s/^\s*dune_artdaq_InhibitMaster\s+(\S+).*/\1/p" $ARTDAQ_DEMO_DIR/ups/product_deps )
dim_version=v20r20
artdaq_dim_plugin_version=v0_02_08a
TRACE_version=v3_13_07
pyzmq_version=v18_0_1a


cd $Base
    cat >setupDUNEARTDAQ_forBuilding <<-EOF
       echo # This script is intended to be sourced.                                                                    
                                                                                                                         
        sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-artdaq.'; exi
t; }" || exit                                                                                           

        export UPS_OVERRIDE="\${UPS_OVERRIDE:--B}"

        source /nfs/sw/artdaq/products/setup                                      
        source /nfs/sw/artdaq/products_dev/setup                                      

        setup mrb v1_14_00
        source $startdir/$localproducts_subdir/setup
                                                                                                                  
        export DAQ_INDATA_PATH=$ARTDAQ_DEMO_DIR/test/Generators:$ARTDAQ_DEMO_DIR/inputData                               
                                                                                                                         
        export DUNEARTDAQ_BUILD=$MRB_BUILDDIR/dune_artdaq                                                            
        export DUNEARTDAQ_REPO="$ARTDAQ_DEMO_DIR"                                                                        
        export FHICL_FILE_PATH=.:\$DUNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH                                           

        # RSIPOS 03/07/18 -> Infiniband workaround for FELIX BR build
        export ICP_ROOT=/nfs/sw/felix/QAT/QAT2.0
        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/felix/QAT/QAT2.0/build
        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/felix/QAT/QAT2.0/qatzip2/qatzip/src

        export DISABLE_DOXYGEN="defined"

        # 26-Apr-2018, KAB: moved the sourcing of mrbSetEnv to *after* all product
        # setups so that we get the right env vars for the source repositories that
        # are part of the mrb build area.
        source mrbSetEnv
        export MRB_INSTALL=$startdir/$localproducts_subdir

echo "Only need to source this file once in the environment; now, to perform builds do the following:"
echo ""
echo "  mrb build -j32  # to build without 'installing' the built code"
echo "  mrb install -j32  # to build and 'install' the built code"
echo "    (where 'install' means copy-to-products-area \"$startdir/$localproducts_subdir\" and use in runtime environment)"
echo ""
echo "*** PLEASE NOTE that we should now use 'mrb install -j32' to build the dune-artdaq software ***"
echo ""

EOF


    cat >setupDUNEARTDAQ_forRunning <<-EOF
       echo # This script is intended to be sourced.                                                                    
                                                                                                                         
        sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-artdaq.'; exit; }" || exit     

        export UPS_OVERRIDE="\${UPS_OVERRIDE:--B}"
        source /nfs/sw/artdaq/products/setup                                      
        source /nfs/sw/artdaq/products_dev/setup                                      
        source $startdir/$localproducts_subdir/setupWithoutMRB
        setup dune_artdaq $dune_artdaq_version -q e15:prof
       
        alias RoutingMaster="routing_master"
                                                                                                           
        export DAQ_INDATA_PATH=$ARTDAQ_DEMO_DIR/test/Generators:$ARTDAQ_DEMO_DIR/inputData                               
                                                                                                                         
        export DUNEARTDAQ_BUILD=$MRB_BUILDDIR/dune_artdaq                                                            
        export DUNEARTDAQ_REPO="$ARTDAQ_DEMO_DIR"                                                                        
        export FHICL_FILE_PATH=.:\$DUNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH                                           


# RSIPOS 03/07/18 -> Infiniband workaround for FELIX BR build
#        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/felix/new-software/multidma/software/external/libfabric/1.4.1.3/x86_64-centos7-gcc62-opt/lib
        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/felix/fabric/build/lib
        #source /nfs/sw/felix/HACK-FELIX-BR-BUILD.sh
        export ICP_ROOT=/nfs/sw/sysadmin/QAT/QAT2.0
        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/sysadmin/QAT/QAT2.0/build
        export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:/nfs/sw/sysadmin/QAT/QAT2.0/qatzip2/qatzip/src

       setup pyzmq $pyzmq_version  -q p2714b
       setup dim $dim_version -q e15
       setup artdaq_dim_plugin $artdaq_dim_plugin_version -q e15:prof:s64

       setup TRACE $TRACE_version
        export TRACE_FILE=/tmp/trace_$trace_file_label
        export TRACE_LIMIT_MS="500,1000,2000"


     # Uncomment the "tonM" line for a given TRACE below which you
        # want to activate. If you don't see the TRACE you want, then
        # add a toffM / tonM combination for it like you see in the
        # other examples below. For more info, take a look at
        # https://twiki.cern.ch/twiki/bin/view/CENF/TRACE

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
echo "shell and 'cd $Base' and 'source setupDUNEARTDAQ_forBuilding' and 'mrb install -j 32'".
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

cat >setupDUNEARTDAQ_forInhibitMaster<<-EOF

        echo # This script is intended to be sourced.

        sh -c "[ `ps $$ | grep bash | wc -l` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-artdaq.'; exit; }" || exit

        export UPS_OVERRIDE="\${UPS_OVERRIDE:--B}"
        source /nfs/sw/artdaq/products/setup
        source /nfs/sw/artdaq/products_dev/setup                                      
        setup dune_artdaq_InhibitMaster $dune_artdaq_InhibitMaster_version -q e15:s64:prof
        setup pyzmq $pyzmq_version  -q p2714b

        setup TRACE $TRACE_version
        export TRACE_FILE=/tmp/trace_$trace_file_label
        export TRACE_LIMIT_MS="500,1000,2000"


EOF

cat >setupDUNEARTDAQ_forTRACE<<-EOF

echo # This script is intended to be sourced.

        sh -c "[ `ps $$ | grep bash | wc -l` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-artdaq.'; exit; }" || exit

        export UPS_OVERRIDE="\${UPS_OVERRIDE:--B}"
        source /nfs/sw/artdaq/products/setup
        source /nfs/sw/artdaq/products_dev/setup                                      
        source $startdir/$localproducts_subdir/setupWithoutMRB

        setup TRACE $TRACE_version
        export TRACE_FILE=/tmp/trace_$trace_file_label
        export TRACE_LIMIT_MS="500,1000,2000"

        # Uncomment the "tonM" line for a given TRACE below which you
        # want to activate. If you don't see the TRACE you want, then
        # add a toffM / tonM combination for it like you see in the
        # other examples below. For more info, take a look at
        # https://twiki.cern.ch/twiki/bin/view/CENF/TRACE

        #toffM 12 -n RequestSender
        #tonM 12 -n RequestSender
 
        #toffM 10 -n CommandableFragmentGenerator
        #tonM 10 -n CommandableFragmentGenerator

EOF

if [[ -e $Base/srcs/dune_artdaq/tools/setupWithoutMRB ]]; then
    cp $Base/srcs/dune_artdaq/tools/setupWithoutMRB $Base/$localproducts_subdir/setupWithoutMRB
else
    echo "Problem: there's no setupWithoutMRB script in $Base/srcs/dune_artdaq; exiting..." >&2
    exit 10
fi



cd $MRB_BUILDDIR

export CETPKG_J=$nprocessors
source /nfs/sw/artdaq/products/setup
source /nfs/sw/artdaq/products_dev/setup                                      

# RSIPOS 03/07/18 -> Infiniband workaround for FELIX BR build
        #source /nfs/sw/felix/HACK-FELIX-BR-BUILD.sh
        export ICP_ROOT=/nfs/sw/felix/QAT/QAT2.0
        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/nfs/sw/felix/QAT/QAT2.0/build
        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/nfs/sw/felix/QAT/QAT2.0/qatzip2/qatzip/src

export DISABLE_DOXYGEN="defined"
export MRB_INSTALL=$startdir/$localproducts_subdir
set +u
source mrbSetEnv
set -u
cp -rp $Base/$localproducts_subdir $startdir
mv $Base/setupDUNEARTDAQ_forRunning $startdir
mv $Base/setupDUNEARTDAQ_forInhibitMaster $startdir
mv $Base/setupDUNEARTDAQ_forTRACE $startdir


mrb install -j$nprocessors                              

installStatus=$?

cd $Base
find build_slf7.x86_64/ -type d | xargs -i chmod g+rwx {}              
find build_slf7.x86_64/ -type f | xargs -i chmod g+rw {}               

cd $startdir
ln -s setupDUNEARTDAQ_forRunning setupDUNEARTDAQ

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

