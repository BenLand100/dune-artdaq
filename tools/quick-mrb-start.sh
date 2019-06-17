#! /bin/bash
# Downloads and installs dune-raw-data as an MRB-controlled repository

# THIS WILL INSTALL THE OFFLINE, I.E., nu-QUALIFIED VERSION OF dune-raw-data

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
    echo "-You've deleted the directories which look like localProducts_dune_raw_data_* and build_slf7.x86_64 (though the versions may have changed since this instruction was written)"
    sleep 5
fi



env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
   usage: `basename $0` [options]
examples: `basename $0` 
          `basename $0` --debug --dune-raw-data-developer --dune-raw-data-develop-branch
--debug       perform a debug build
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
	    -dune-raw-data-develop-branch) opt_lrd_develop=1;;
	    -dune-raw-data-developer)  opt_lrd_w=1;;
            *)          echo "Unknown option -$op"; do_help=1;;
        esac
    else
        aa=`echo "$1" | sed -e"s/'/'\"'\"'/g"` args="$args '$aa'"; shift
    fi
done
eval "set -- $args \"\$@\""; unset args aa

set -u   # complain about uninitialed shell variables - helps development

if [[ $opt_lrd_develop -eq 0 ]]; then
    echo "JCF, May-12-2017: currently there isn't an official cut release of dune-raw-data; therefore you need to supply the --dune-raw-data-develop-branch argument to this script" >&2
    exit 1
fi

dune_repo=/cvmfs/dune.opensciencegrid.org/products/dune

if [[ ! -e $dune_repo ]]; then
    echo "This installation needs access to the CVMFS mount point for the dune repo, ${dune_repo}, in order to obtain the dunepdsprce packages. Aborting..." >&2
    exit 1
fi

source $dune_repo/setup

larsoft_repo=/cvmfs/fermilab.opensciencegrid.org/products/larsoft

if [[ -e $larsoft_repo ]]; then
    source $larsoft_repo/setup
fi

test -n "${do_help-}" -o $# -ge 2 && echo "$USAGE" && exit

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
   wget https://cdcvs.fnal.gov/redmine/projects/dune-raw-data/repository/revisions/$tag/raw/ups/product_deps
fi

if [[ ! -e $Base/download/product_deps ]]; then
   echo "You need to have a product_deps file in $Base/download" >&2
   exit 1
fi

coredemo_version=`grep "parent dune_raw_data" $Base/download/product_deps|awk '{print $3}'`

default_quals=$( awk '/^defaultqual/ {print $2} ' $Base/download/product_deps )
default_quals=$( echo $default_quals | sed -r 's/online/nu/' )

quals=$default_quals

defaultE=$( echo $quals |  sed -r 's/.*(e[0-9]+).*/\1/' )
defaultS=$( echo $quals |  sed -r 's/.*(s[0-9]+).*/\1/' )

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

artdaq_core_version=$( sed -r -n 's/artdaq_core\s+(.*)\s+nu.*/\1/p'  $Base/download/product_deps )
art_version=$( sed -r -n 's/art\s+(.*)\s+nu.*/\1/p' $Base/download/product_deps)
TRACE_version=$( sed -r -n 's/TRACE\s+(.*)\s+nu.*/\1/p' $Base/download/product_deps)
cetbuildtools_version=$( sed -r -n 's/cetbuildtools\s+(.*)\s+nu.*/\1/p' $Base/download/product_deps)


artdaq_core_version_dot=$( echo $artdaq_core_version | sed -r 's/v//;s/_/./g' )
art_version_dot=$( echo $art_version | sed -r 's/v//;s/_/./g' )
TRACE_version_dot=$( echo $TRACE_version | sed -r 's/v//;s/_/./g' )
cetbuildtools_version_dot=$( echo $cetbuildtools_version | sed -r 's/v//;s/_/./g' )

# If we aren't connected to the outside world, you'll need to have
# previously scp'd or rsync'd the products to the host you're trying
# to install dune-artdaq on

if ! $bad_network; then
    wget http://scisoft.fnal.gov/scisoft/bundles/tools/pullProducts
    chmod +x pullProducts

    ./pullProducts $Base/products ${os} art-${art_version} ${equalifier} ${build_type}

    if [ $? -ne 0 ]; then
	echo "Error in pullProducts. Please go to http://scisoft.fnal.gov/scisoft/bundles/art/${art_version}/manifest and make sure that a manifest for the specified qualifier (${equalifier}) exists."
	exit 1
    fi

    detectAndPull mrb noarch

    curl -O http://scisoft.fnal.gov/scisoft/packages/artdaq_core/$artdaq_core_version/artdaq_core-${artdaq_core_version_dot}-${os}-x86_64-${equalifier}-${squalifier}-${build_type}.tar.bz2
    curl -O http://scisoft.fnal.gov/scisoft/packages/cetbuildtools/$cetbuildtools_version/cetbuildtools-${cetbuildtools_version_dot}-noarch.tar.bz2
    curl -O http://scisoft.fnal.gov/scisoft/packages/TRACE/$TRACE_version/TRACE-${TRACE_version_dot}-${os}-x86_64.tar.bz2
fi

cd $Base/products

echo "Unzipping downloaded archive files; this may take a while..."

for archivefile in $Base/download/*.bz2; do
    echo tar xjf $archivefile
    tar xjf $archivefile
done

source $Base/products/setup
setup mrb
setup git
setup gitflow

export MRB_PROJECT=dune_raw_data
cd $Base
mrb newDev -f -v $coredemo_version -q ${quals}:${build_type}
set +u
localproducts_setup=$Base/localProducts_dune_raw_data_${coredemo_version}_$( echo $quals | tr ':' '_' )_${build_type}/setup
source $localproducts_setup
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

if ! $bad_network; then
    

    mrb gitCheckout $dune_raw_data_checkout_arg $dune_raw_data_repo

    if [[ "$?" != "0" ]]; then
	echo "Unable to perform checkout of $dune_raw_data_repo"
	exit 1
    fi

fi

sed -i -r '/^defaultqual/s/online/nu/' $Base/srcs/dune_raw_data/ups/product_deps

ARTDAQ_DEMO_DIR=$Base/srcs/dune_raw_data
cd $Base
    cat >setupDUNERAWDATA <<-EOF
       echo # This script is intended to be sourced.                                                                    
                                                                                                                         
        sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running dune-raw-data.'; exit; }" || exit                                                                                           
        source $Base/products/setup                                                                                   
        source $dune_repo/setup

        if [[ -e $larsoft_repo ]]; then
             source $larsoft_repo/setup
        fi

        setup mrb
        source $localproducts_setup
        source mrbSetEnv       
                                                                                                                  
        export DAQ_INDATA_PATH=$ARTDAQ_DEMO_DIR/test/Generators:$ARTDAQ_DEMO_DIR/inputData                               
                                                                                                                         
        export DUNERAWDATA_BUILD=$MRB_BUILDDIR/dune_raw_data                                                            
        export DUNERAWDATA_REPO="$ARTDAQ_DEMO_DIR"                                                                                    
        setup TRACE $TRACE_version
        setup art $art_version -q ${equalifier}:${build_type}
        setup artdaq_core $artdaq_core_version -q ${equalifier}:${build_type}:${squalifier}


	EOF
    #

source $dune_repo/setup
source $Base/products/setup   

cd $MRB_BUILDDIR
set +u
source mrbSetEnv
set -u
setup TRACE $TRACE_version
setup art $art_version -q ${equalifier}:${build_type}
setup artdaq_core $artdaq_core_version -q ${equalifier}:${build_type}:${squalifier}


export CETPKG_J=$((`cat /proc/cpuinfo|grep processor|tail -1|awk '{print $3}'` + 1))
mrb build    # VERBOSE=1
installStatus=$?

if [ $installStatus -eq 0 ]; then
    echo "dune-raw-data has been installed correctly."
    echo
else
    echo "Build error. If all else fails, try (A) logging into a new terminal and (B) creating a new directory out of which to run this script."
    echo
fi

endtime=`date`

echo "Build start time: $starttime"
echo "Build end time:   $endtime"

