#! /bin/bash
# quick-mrb-start.sh - Eric Flumerfelt, May 20, 2016
# Downloads, installs, and runs the artdaq_demo as an MRB-controlled repository

# JCF, Jan-1-2017
# Modified this script to work with the lbne-artdaq package

# JCF, Mar-2-2017
# Modified it again to work with the brand new dune-artdaq package

export USER=${USER:-$(whoami)}
export HOSTNAME=${HOSTNAME:-$(hostname)}

if [[ "$USER" == "np04daq" ]]; then
    
    cat<<EOF >&2

    Unless the dune-artdaq installation you're creating is meant for
    standard experiment-wide running rather than for code development
    purposes, you should run this script under your own user account,
    not under the np04daq account

EOF

exit 1
fi

if [[ $HOSTNAME != np04-srv-023 ]]; then
    
    cat<<EOF >&2

As of Jul-13-2019, this script is designed to only be run on
np04-srv-023, since that's the node that has the local /scratch disk
where your source code and local build area will be placed. You appear
to be on host $HOSTNAME. Exiting...

EOF
    exit 1
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

startdir=$PWD

if ! [[ "$startdir" =~ ^/nfs/sw/work_dirs ]]; then

    cat<<EOF >&2

Installation needs to be performed in a subdirectory of
/nfs/sw/work_dirs (and preferably in a subdirectory of
/nfs/sw/work_dirs/$USER). Exiting...

EOF

    exit 1
fi

if [[ -n $( ls -a1 | grep -E -v "^quick-mrb-start.*" | grep -E -v "^\.\.?$" ) ]]; then

    cat<<EOF >&2

There appear to be files in $startdir besides this script; this script
should only be run in a clean directory. Exiting...

EOF
    exit 1
fi

qms_tmpdir=/tmp/${USER}_for_quick-mrb-start
mkdir -p $qms_tmpdir
    
if [[ "$?" != "0" ]]; then
    echo "There was a problem running mkdir -p ${qms_tmpdir}, needed by this script for stashing temporary files; exiting..." >&2
    exit 1
fi

returndir=$PWD
cd $qms_tmpdir
rm -f quick-mrb-start.sh
wget https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/repository/revisions/develop/raw/tools/quick-mrb-start.sh

quick_mrb_start_edits=$( diff $startdir/quick-mrb-start.sh $qms_tmpdir/quick-mrb-start.sh ) 
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

localdiskdir=$( echo $startdir | sed -r 's!/nfs/sw/work_dirs!/scratch!' )
if [[ ! -e $localdiskdir ]]; then
    mkdir -p $localdiskdir
    if [[ ! -e $localdiskdir ]]; then
	echo "Error! Problem calling mkdir -p ${localdiskdir}. Exiting..." >&2
	exit 1
    fi
    cd $localdiskdir
else
    echo "Error: this script wanted to install a dune-artdaq area called $localdiskdir, but it appears that directory already exists! Exiting..." >&2
    exit 1
fi

starttime=`date`

test -d products || mkdir products
test -d download || mkdir download
test -d log || mkdir log


# JCF, 1/16/15
# Save all output from this script (stdout + stderr) in a file with a
# name that looks like "quick-start.sh_Fri_Jan_16_13:58:27.script" as
# well as all stderr in a file with a name that looks like
# "quick-start.sh_Fri_Jan_16_13:58:27_stderr.script"
alloutput_file=$( date | awk -v "SCRIPTNAME=$(basename $0)" '{print SCRIPTNAME"_"$1"_"$2"_"$3"_"$4".script"}' )
stderr_file=$( date | awk -v "SCRIPTNAME=$(basename $0)" '{print SCRIPTNAME"_"$1"_"$2"_"$3"_"$4"_stderr.script"}' )
exec  > >(tee "$localdiskdir/log/$alloutput_file")
exec 2> >(tee "$localdiskdir/log/$stderr_file")

# Get all the information we'll need to decide which exact flavor of the software to install
if [ -z "${tag:-}" ]; then 
  tag=develop;
fi

returndir=$PWD
cd $qms_tmpdir
rm -f product_deps
wget -O product_deps https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/repository/raw/ups/product_deps?rev=$( echo $dune_artdaq_branch | sed -r 's!/!%2F!g' )

if [[ "$?" != "0" ]]; then
    echo "Problem trying to get product_deps file for branch $dune_artdaq_branch off the web, needed to determine package versions; exiting..." >&2
    exit 1
fi

demo_version=$( grep "parent dune_artdaq" ./product_deps|awk '{print $3}' )
artdaq_version=$( grep -E "^artdaq\s+" ./product_deps | awk '{print $2}' )

cd $returndir

equalifier=e15
squalifier=s64

if [[ -n "${opt_debug:-}" ]] ; then
    build_type="debug"
else
    build_type="prof"
fi

# JCF, Apr-26-2018: Kurt discovered that it's necessary for the products area to be based on ups v6_0_7
upsfile=/nfs/sw/control_files/misc/ups-6.0.7-Linux64bit+3.10-2.17.tar.bz2

if [[ ! -e $upsfile ]]; then
    echo "Unable to find ${upsfile}; you'll need to restore it from SciSoft" >&2
    exit 1
fi

cd $localdiskdir/products
cp -p $upsfile .
tar xjf $upsfile
source $localdiskdir/products/setup
setup mrb v1_14_00
setup git
setup gitflow

export MRB_PROJECT=dune_artdaq
export localproducts_subdir=localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}
cd $localdiskdir
mrb newDev -f -v $demo_version -q ${equalifier}:${build_type}
set +u

source $localdiskdir/$localproducts_subdir/setup
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

ARTDAQ_DEMO_DIR=$localdiskdir/srcs/dune_artdaq


nprocessors=$( grep -E "processor\s+:" /proc/cpuinfo | wc -l )
trace_file_label=$( basename $localdiskdir )

dune_artdaq_InhibitMaster_version=$( sed -r -n "s/^\s*dune_artdaq_InhibitMaster\s+(\S+).*/\1/p" $ARTDAQ_DEMO_DIR/ups/product_deps )
dim_version=v20r20
artdaq_dim_plugin_version=v0_02_08d
TRACE_version=v3_13_07
pyzmq_version=v18_0_1a


cd $localdiskdir
    cat >setupDUNEARTDAQ_forBuilding <<-EOF

        basedir=\$PWD                                                                                  
        
        if [[ -d \$basedir/srcs ]]; then
          cd \$basedir/srcs/dune_artdaq
          dune_artdaq_branch=\$( git branch | sed -r -n 's/^\*\s+(.*)/\1/p' )
          cd \$basedir/srcs/dune_raw_data
          dune_raw_data_branch=\$( git branch | sed -r -n 's/^\*\s+(.*)/\1/p' )
          cd \$basedir
          
          if [[ "\$dune_artdaq_branch" == "develop" && "\$dune_raw_data_branch" == "for_dune-artdaq" ]]; then
           echo "Both git repositories: " 
           echo "\$basedir/srcs/dune_artdaq"
           echo "and" 
           echo "\$basedir/srcs/dune_raw_data "
           echo "...are currently set to the *release* branches (\"develop\" and \"for_dune-artdaq\", respectively)."
           echo "Code development should be performed on non-release branches; please switch to a different "
           echo "branch for the repo(s) you're developing in. Returning without setting up the build environment..."
           echo
           return
          fi

        else
          echo "Unable to find the subdirectory containing git repos, ./srcs"
          echo "This script is meant to be sourced from the base of an mrb area; returning..." >&2
          echo
          return
        fi
                               
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
echo "  mrb install -j32  # to build and 'install' the built code"
echo " (where 'install' means copy-to-products-area \"$startdir/$localproducts_subdir\" and use in runtime environment)"
echo ""

EOF

cd $startdir
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

echo ""
echo "You have just sourced setupDUNEARTDAQ_forRunning. This is probably "
echo "not what you want, since this script is meant to be sourced by JCOP, "
echo "rather than a human, in order to set up the running environment "
echo ""
echo "If, instead, you would like to build the software, please start with a fresh"
echo "shell and 'cd $localdiskdir' and 'source setupDUNEARTDAQ_forBuilding' and 'mrb install -j 32'".
echo "Also see https://twiki.cern.ch/twiki/bin/view/CENF/Building for more details on building"
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

if [[ -e $localdiskdir/srcs/dune_artdaq/tools/setupWithoutMRB ]]; then
    cp $localdiskdir/srcs/dune_artdaq/tools/setupWithoutMRB $localdiskdir/$localproducts_subdir/setupWithoutMRB
else
    echo "Problem: there's no setupWithoutMRB script in $localdiskdir/srcs/dune_artdaq; exiting..." >&2
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
cp -rp $localdiskdir/$localproducts_subdir $startdir

mrb install -j$nprocessors                              

installStatus=$?

cd $startdir
ln -s setupDUNEARTDAQ_forRunning setupDUNEARTDAQ

if [ $installStatus -eq 0 ]; then

    echo "dune-artdaq has been installed correctly."

else
    cat <<EOF >&2

Build error. Possible reasons for this include:

-Lack of agreement between dune-artdaq and dune-raw-data package
 dependencies due to out-of-sync branches

-Running this script in an unclean environment (i.e., one with
 products already set up, scripts sourced, etc.)

Look in the record of this installation attempt, found in
$localdiskdir/log, for further details.

EOF

fi

cat<<EOF

To rebuild, go to $localdiskdir and then follow the instructions
at https://twiki.cern.ch/twiki/bin/view/CENF/Building. Note the
products you build (dune-artdaq, etc.) will be installed in
$startdir on the NFS disk so they can be used on all np04 nodes
during running.

EOF


echo                                                               

endtime=`date`

echo "Build start time: $starttime"
echo "Build end time:   $endtime"

