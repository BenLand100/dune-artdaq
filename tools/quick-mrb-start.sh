#! /bin/bash
# quick-mrb-start.sh - Eric Flumerfelt, May 20, 2016
# Downloads, installs, and runs the artdaq_demo as an MRB-controlled repository

# JCF, Jan-1-2017
# Modified this script to work with the lbne-artdaq package

# JCF, Mar-2-2017
# Modified it again to work with the brand new dune-artdaq package

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
          `basename $0` --wib-installation-dir <dirname1> --uhal-products-dir <dirname2> --dune-raw-data-developer --dune-raw-data-develop-branch
          `basename $0` --wib-installation-dir <dirname1> --uhal-products-dir <dirname2> --debug --noviewer --dune-raw-data-developer
--wib-installation-dir <dirname1> provide location of installation of WIB software from Boston University (mandatory)
--uhal-products-dir <dirname2> provide location of products directory containing uhal (mandatory)
--debug       perform a debug build
--noviewer    skip installion of artdaq Message Viewer (use if there is no XWindows)
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
	    -uhal-products-dir)   uhal_products_dir="$1";;
	    -wib-installation-dir) wib_installation_dir="$1";;
	    -debug)     opt_debug=--debug;;
	    -noviewer)    opt_noviewer=--noviewer;;
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

if [[ -z $uhal_products_dir ]]; then
    echo "Must supply the name of the products directory containing the uhal package needed for the timing fragment generator" >&2
    exit 1
fi

if [[ ! -e $uhal_products_dir ]]; then
    echo "Unable to find products directory ${uhal_products_dir}; exiting..." >&2
    exit 1
fi

. $uhal_products_dir/setup
uhal_setup_cmd="setup uhal v2_4_2 -q e14:prof:s50"
$uhal_setup_cmd

if [[ "$?" != "0" ]]; then
    echo "Error: command \"${uhal_setup_cmd}\" failed. uhal needs to be set up in order for the timing fragment generator to build. Is the appropriate uhal version located in $uhal_products_dir ?" >&2
    exit 1
fi


if [[ -z $wib_installation_dir ]]; then
    echo "Must supply the name of the directory which contains the dune-wib source, shared object libraries, etc." >&2
    exit 1
fi

if [[ ! -e $wib_installation_dir ]]; then 
    echo "Unable to find WIB software installation directory ${wib_installation_dir}; exiting..." >&2
    exit 1
fi

# If WIB_DIRECTORY changes here, make sure it also changes in setupDUNEARTDAQ
export WIB_DIRECTORY=$wib_installation_dir/WIB

set -u   # complain about uninitialed shell variables - helps development

if [[ $opt_lrd_develop -eq 0 ]]; then
    echo "JCF, May-12-2017: currently there isn't an official cut release of dune-raw-data; therefore you need to supply the --dune-raw-data-develop-branch argument to this script" >&2
    exit 1
fi

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
    wget https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/repository/revisions/$tag/raw/ups/product_deps
fi

if [[ ! -e $Base/download/product_deps ]]; then
    echo "You need to have a product_deps file in $Base/download" >&2
    exit 1
fi


demo_version=`grep "parent dune_artdaq" $Base/download/product_deps|awk '{print $3}'`

artdaq_version=`grep -E "^artdaq\s+" $Base/download/product_deps | awk '{print $2}'`
coredemo_version=`grep -E "^dune_raw_data\s+" $Base/download/product_deps | awk '{print $2}'`
gallery_version=`grep -E "^gallery\s+" $Base/download/product_deps | awk '{print $2}'`

default_quals_cmd="sed -r -n 's/.*(e[0-9]+):(s[0-9]+).*/\1 \2/p' $Base/download/product_deps | uniq"

if [[ $(eval $default_quals_cmd | wc -l) -gt 1 ]]; then
    echo "More than one build qualifier combination found in download/product_deps; please contact John Freeman at jcfree@fnal.gov" >&2
    exit 1
fi

defaultE=$( eval $default_quals_cmd | awk '{print $1}' )
defaultS=$( eval $default_quals_cmd | awk '{print $2}' )

if [ -n "${equalifier-}" ]; then 
    equalifier="e${equalifier}";
else
    equalifier=$defaultE
fi
if [ -n "${squalifier-}" ]; then
    squalifier="s${squalifier}"
else
    squalifier=$defaultS
fi
if [[ -n "${opt_debug:-}" ]] ; then
    build_type="debug"
else
    build_type="prof"
fi

# JCF, Jan-1-2016

# The gallery package is currently something only dune-artdaq depends
# on, hence we can't rely on pullProducts to get it for us

if ! $bad_network; then
os=`$Base/download/cetpkgsupport/bin/get-directory-name os`
detectAndPull gallery ${os}-x86_64 "${equalifier}-${build_type}" $gallery_version
fi

# If we aren't connected to the outside world, you'll need to have
# previously scp'd or rsync'd the products to the host you're trying
# to install dune-artdaq on

if ! $bad_network; then
    wget http://scisoft.fnal.gov/scisoft/bundles/tools/pullProducts
    chmod +x pullProducts
    ./pullProducts $Base/products ${os} artdaq-${artdaq_version} ${squalifier}-${equalifier} ${build_type}

    if [ $? -ne 0 ]; then
	echo "Error in pullProducts. Please go to http://scisoft.fnal.gov/scisoft/bundles/artdaq/${artdaq_version}/manifest and make sure that a manifest for the specified qualifiers (${squalifier}-${equalifier}) exists."
	exit 1
    fi

    detectAndPull mrb noarch
fi

source $Base/products/setup
setup mrb
setup git
setup gitflow

export MRB_PROJECT=dune_artdaq
cd $Base
mrb newDev -f -v $demo_version -q ${equalifier}:${build_type}
set +u
source $Base/localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}/setup
set -u

cd $MRB_SOURCE

if [[ $opt_lrd_develop -eq 1 ]]; then
    dune_raw_data_checkout_arg="-d dune_raw_data"
else
    dune_raw_data_checkout_arg="-t ${coredemo_version} -d dune_raw_data"
fi


if [[ $opt_lrd_w -eq 1 ]]; then
    dune_raw_data_repo="ssh://p-dune-raw-data@cdcvs.fnal.gov/cvs/projects/dune-raw-data"
else
    dune_raw_data_repo="http://cdcvs.fnal.gov/projects/dune-raw-data"
fi

if [[ $tag == "develop" ]]; then
    dune_artdaq_checkout_arg="-d dune_artdaq"
else
    dune_artdaq_checkout_arg="-t $tag -d dune_artdaq"
fi


# Notice the default for write access to dune-artdaq is the opposite of that for dune-raw-data
if [[ $opt_la_nw -eq 1 ]]; then
    dune_artdaq_repo="http://cdcvs.fnal.gov/projects/dune-artdaq"
else
    dune_artdaq_repo="ssh://p-dune-artdaq@cdcvs.fnal.gov/cvs/projects/dune-artdaq"
fi


if ! $bad_network; then

    mrb gitCheckout -t ${artdaq_version} -d artdaq http://cdcvs.fnal.gov/projects/artdaq

    if [[ "$?" != "0" ]]; then
	echo "Unable to perform checkout of http://cdcvs.fnal.gov/projects/artdaq"
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
fi

# JCF, Jun-13-2017

# The following modification of product_deps is Cargo Cult
# Programming: apparently, mrbSetEnv seems to look at the default qual
# in artdaq's ups/product_deps file (e10:s46), even if the default
# qual in dune-artdaq implies a different qualifier (e14:s48) for
# artdaq. Passing the option "-q e14:prof" to mrb newDev beforehand
# doesn't help, and as mrbSetEnv itself is unreadable, I've arrived at
# the following "patch" through trial-and-error:

if [[ ! -e artdaq/ups/product_deps ]]; then
    echo "Can't find artdaq/ups/product_deps; you need to be in the srcs/ directory"
    exit 1
fi

sed -i -r 's/^\s*defaultqual(\s+).*/defaultqual\1'$equalifier':'$squalifier'/' artdaq/ups/product_deps

if ! $bad_network && [[ "x${opt_noviewer-}" == "x" ]] ; then 

    cd $MRB_SOURCE
    mfextensionsver=$( awk '/^[[:space:]]*artdaq_mfextensions/ { print $2 }' artdaq/ups/product_deps )

    git clone http://cdcvs.fnal.gov/projects/mf-extensions-git
    cd mf-extensions-git
    git checkout $mfextensionsver
    cd ..

    qtver=$( awk '/^[[:space:]]*qt[[:space:]]*/ {print $2}' mf-extensions-git/ups/product_deps )

    rm -rf mf-extensions-git

    os=`$Base/download/cetpkgsupport/bin/get-directory-name os`

    # qt v5_7_1 uses the token "sl7", and artdaq_mfextensions uses the
    # token "slf7", as I found out the hard way

    detectAndPull artdaq_mfextensions ${os}-x86_64 ${equalifier}-${squalifier}-${build_type} $mfextensionsver

    if [[ "$os" == "slf7" ]]; then
     	os="sl7"
    fi

    detectAndPull qt ${os}-x86_64 ${equalifier} ${qtver}
fi

ARTDAQ_DEMO_DIR=$Base/srcs/dune_artdaq
cd $Base
    cat >setupDUNEARTDAQ <<-EOF
       echo # This script is intended to be sourced.                                                                    
                                                                                                                         
        sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-artdaq.'; exit; }" || exit                                                                                           
        source ${uhal_products_dir}/setup                                      
        source $Base/products/setup                                                                                   
        setup mrb
        source $Base/localProducts_dune_artdaq_${demo_version}_${equalifier}_${build_type}/setup
        source mrbSetEnv       
                                                                                                                  
        export DAQ_INDATA_PATH=$ARTDAQ_DEMO_DIR/test/Generators:$ARTDAQ_DEMO_DIR/inputData                               
                                                                                                                         
        export DUNEARTDAQ_BUILD=$MRB_BUILDDIR/dune_artdaq                                                            
        export DUNEARTDAQ_REPO="$ARTDAQ_DEMO_DIR"                                                                        
        export FHICL_FILE_PATH=.:\$DUNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH                                           
        export WIB_DIRECTORY=${wib_installation_dir}/WIB
        ${uhal_setup_cmd}                                                                                                                          
# JCF, 11/25/14                                                                                                          
# Make it easy for users to take a quick look at their output file via "rawEventDump"                                    
                                                                                                                         
alias rawEventDump="art -c \$DUNEARTDAQ_REPO/tools/fcl/rawEventDump.fcl "                                                                                        

	EOF
    #

# Build artdaq_demo
cd $MRB_BUILDDIR
set +u
source mrbSetEnv
set -u
export CETPKG_J=$((`cat /proc/cpuinfo|grep processor|tail -1|awk '{print $3}'` + 1))
mrb build    # VERBOSE=1
installStatus=$?

if [ $installStatus -eq 0 ] && [ "x${opt_run_demo-}" != "x" ]; then
    echo doing the demo

    toolsdir=${DUNE_ARTDAQ_DIR}/tools

    . $toolsdir/run_demo.sh $Base $toolsdir

elif [ $installStatus -eq 0 ]; then
    echo "dune-artdaq has been installed correctly."
    echo
else
    echo "Build error. If all else fails, try (A) logging into a new terminal and (B) creating a new directory out of which to run this script."
    echo
fi

endtime=`date`

echo "Build start time: $starttime"
echo "Build end time:   $endtime"

