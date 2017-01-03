#! /bin/bash
# quick-mrb-start.sh - Eric Flumerfelt, May 20, 2016
# Downloads, installs, and runs the artdaq_demo as an MRB-controlled repository

# JCF, Jan-1-2017
# Modified this script to work with the lbne-artdaq package

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

env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
   usage: `basename $0` [options] [demo_root]
examples: `basename $0` .
          `basename $0` --run-demo
          `basename $0` --debug
          `basename $0` --tag v2_08_04
If the \"demo_root\" optional parameter is not supplied, the user will be
prompted for this location.
--run-demo    runs the demo
--debug       perform a debug build
--develop     Install the develop version of the software (may be unstable!)
--viewer      install and run the artdaq Message Viewer
--tag         Install a specific tag of lbne_artdaq
-e, -s        Use specific qualifiers when building ARTDAQ
-v            Be more verbose
-x            set -x this script
-w            Check out repositories read/write
"

# Process script arguments and options
eval env_opts=\${$env_opts_var-} # can be args too
eval "set -- $env_opts \"\$@\""
op1chr='rest=`expr "$op" : "[^-]\(.*\)"`   && set -- "-$rest" "$@"'
op1arg='rest=`expr "$op" : "[^-]\(.*\)"`   && set --  "$rest" "$@"'
reqarg="$op1arg;"'test -z "${1+1}" &&echo opt -$op requires arg. &&echo "$USAGE" &&exit'
args= do_help= opt_v=0; opt_w=0; opt_develop=0;
while [ -n "${1-}" ];do
    if expr "x${1-}" : 'x-' >/dev/null;then
        op=`expr "x$1" : 'x-\(.*\)'`; shift   # done with $1
        leq=`expr "x$op" : 'x-[^=]*\(=\)'` lev=`expr "x$op" : 'x-[^=]*=\(.*\)'`
        test -n "$leq"&&eval "set -- \"\$lev\" \"\$@\""&&op=`expr "x$op" : 'x\([^=]*\)'`
        case "$op" in
            \?*|h*)     eval $op1chr; do_help=1;;
            v*)         eval $op1chr; opt_v=`expr $opt_v + 1`;;
            x*)         eval $op1chr; set -x;;
            s*)         eval $op1arg; squalifier=$1; shift;;
            e*)         eval $op1arg; equalifier=$1; shift;;
			w*)         eval $op1chr; opt_w=`expr $opt_w + 1`;;
            -run-demo)  opt_run_demo=--run-demo;;
	    -debug)     opt_debug=--debug;;
			-develop) opt_develop=1;;
			-tag)       eval $reqarg; tag=$1; shift;;
	    -viewer)    opt_viewer=--viewer;;
            *)          echo "Unknown option -$op"; do_help=1;;
        esac
    else
        aa=`echo "$1" | sed -e"s/'/'\"'\"'/g"` args="$args '$aa'"; shift
    fi
done
eval "set -- $args \"\$@\""; unset args aa
set -u   # complain about uninitialed shell variables - helps development

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

cd $Base/download

echo "Cloning cetpkgsupport to determine current OS"
git clone http://cdcvs.fnal.gov/projects/cetpkgsupport
os=`./cetpkgsupport/bin/get-directory-name os`

if [[ "$os" == "u14" ]]; then
	echo "-H Linux64bit+3.19-2.19" >../products/ups_OVERRIDE.`hostname`
fi

# Get all the information we'll need to decide which exact flavor of the software to install
notag=0
if [ -z "${tag:-}" ]; then 
  tag=develop;
  notag=1;
fi
wget https://cdcvs.fnal.gov/redmine/projects/lbne-artdaq/repository/revisions/$tag/raw/ups/product_deps
demo_version=`grep "parent lbne_artdaq" $Base/download/product_deps|awk '{print $3}'`
if [ $notag -eq 1 ];then
  tag=$demo_version
fi
artdaq_version=`grep -E "^artdaq\s+" $Base/download/product_deps | awk '{print $2}'`
coredemo_version=`grep -E "^lbne_raw_data\s+" $Base/download/product_deps | awk '{print $2}'`
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

# The gallery package is currently something only lbne-artdaq depends
# on, hence we can't rely on pullProducts to get it for us

os=`$Base/download/cetpkgsupport/bin/get-directory-name os`
detectAndPull gallery ${os}-x86_64 "${equalifier}-${build_type}" $gallery_version

wget http://scisoft.fnal.gov/scisoft/bundles/tools/pullProducts
chmod +x pullProducts
./pullProducts $Base/products ${os} artdaq-${artdaq_version} ${squalifier}-${equalifier} ${build_type}
   if [ $? -ne 0 ]; then
	echo "Error in pullProducts. Please go to http://scisoft.fnal.gov/scisoft/bundles/artdaq/${artdaq_version}/manifest and make sure that a manifest for the specified qualifiers (${squalifier}-${equalifier}) exists."
	exit 1
   fi

detectAndPull mrb noarch
source $Base/products/setup
setup mrb
setup git
setup gitflow

export MRB_PROJECT=lbne_artdaq
cd $Base
mrb newDev -f -v $demo_version -q ${equalifier}:${build_type}
set +u
source $Base/localProducts_lbne_artdaq_${demo_version}_${equalifier}_${build_type}/setup
set -u

cd $MRB_SOURCE
if [[ $opt_develop -eq 1 ]]; then
if [ $opt_w -gt 0 ];then
mrb gitCheckout -t ${artdaq_version} -d artdaq ssh://p-artdaq@cdcvs.fnal.gov/cvs/projects/artdaq
mrb gitCheckout -d lbne_raw_data ssh://p-lbne-raw-data@cdcvs.fnal.gov/cvs/projects/lbne-raw-data
mrb gitCheckout -d lbne_artdaq ssh://p-lbne-artdaq@cdcvs.fnal.gov/cvs/projects/lbne-artdaq
else
mrb gitCheckout -t ${artdaq_version} -d artdaq http://cdcvs.fnal.gov/projects/artdaq
mrb gitCheckout -d lbne_raw_data http://cdcvs.fnal.gov/projects/lbne-raw-data
mrb gitCheckout -d lbne_artdaq http://cdcvs.fnal.gov/projects/lbne-artdaq
fi
else
if [ $opt_w -gt 0 ];then
mrb gitCheckout -t ${artdaq_version} -d artdaq ssh://p-artdaq@cdcvs.fnal.gov/cvs/projects/artdaq
mrb gitCheckout -t ${coredemo_version} -d lbne_raw_data ssh://p-lbne-raw-data@cdcvs.fnal.gov/cvs/projects/lbne-raw-data
mrb gitCheckout -t ${demo_version} -d lbne_artdaq ssh://p-lbne-artdaq@cdcvs.fnal.gov/cvs/projects/lbne-artdaq
else
mrb gitCheckout -t ${artdaq_version} -d artdaq http://cdcvs.fnal.gov/projects/artdaq
mrb gitCheckout -t ${coredemo_version} -d lbne_raw_data http://cdcvs.fnal.gov/projects/lbne-raw-data
mrb gitCheckout -t ${demo_version} -d lbne_artdaq http://cdcvs.fnal.gov/projects/lbne-artdaq
fi
fi


if [[ "x${opt_viewer-}" != "x" ]]; then
    os=`$Base/download/cetpkgsupport/bin/get-directory-name os`
    detectAndPull qt ${os}-x86_64 ${equalifier} v5_6_1a
    cd $MRB_SOURCE
    mrb gitCheckout -d artdaq_mfextensions http://cdcvs.fnal.gov/projects/mf-extensions-git
fi

ARTDAQ_DEMO_DIR=$Base/srcs/lbne_artdaq
cd $Base
    cat >setupLBNEARTDAQ <<-EOF
       echo # This script is intended to be sourced.                                                                    
                                                                                                                         
        sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running lbne-artdaq.'; exit; }" || exit                                                                                           
                                      
        source $Base/products/setup                                                                                   
        setup mrb
        source $Base/localProducts_lbne_artdaq_${demo_version}_${equalifier}_${build_type}/setup
        source mrbSetEnv       
                                                                                                                  
        export DAQ_INDATA_PATH=$ARTDAQ_DEMO_DIR/test/Generators:$ARTDAQ_DEMO_DIR/inputData                               
                                                                                                                         
        export LBNEARTDAQ_BUILD=$MRB_BUILDDIR/lbne_artdaq                                                            
        export LBNEARTDAQ_REPO="$ARTDAQ_DEMO_DIR"                                                                        
        export FHICL_FILE_PATH=.:\$LBNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH                                           
                                                                                                                         
# JCF, 11/25/14                                                                                                          
# Make it easy for users to take a quick look at their output file via "rawEventDump"                                    
                                                                                                                         
alias rawEventDump="art -c \$LBNEARTDAQ_REPO/tools/fcl/rawEventDump.fcl "                                                                                        

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

    toolsdir=${LBNE_ARTDAQ_DIR}/tools

    . $toolsdir/run_demo.sh $Base $toolsdir

elif [ $installStatus -eq 0 ]; then
    echo "lbne-artdaq has been installed correctly."
    echo
else
    echo "Build error. If all else fails, try (A) logging into a new terminal and (B) creating a new directory out of which to run this script."
    echo
fi

endtime=`date`

echo "Build start time: $starttime"
echo "Build end time:   $endtime"
