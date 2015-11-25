#!/bin/bash
echo Invoked: $0 "$@"
env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
  usage: `basename $0` [options] <demo_products_dir/> <lbne-artdaq/>
example: `basename $0` products lbne-artdaq --run-demo
<demo_products>    where products were installed (products/)
<lbne-artdaq_root> directory where lbne-artdaq was cloned into.
--run-demo                       runs the demo
--debug                          perform debug builds
--not-lbne-raw-data-developer    checkout lbne-raw-data using read-only http
--not-artdaq-developer           checkout artdaq using read-only http
Currently this script will clone (if not already cloned) artdaq
along side of the lbne-artdaq dir.
Also it will create, if not already created, build directories
for artdaq, lbne-artdaq, and lbne-raw-data.
"
# Process script arguments and options
eval env_opts=\${$env_opts_var-} # can be args too
eval "set -- $env_opts \"\$@\""
op1chr='rest=`expr "$op" : "[^-]\(.*\)"`   && set -- "-$rest" "$@"'
op1arg='rest=`expr "$op" : "[^-]\(.*\)"`   && set --  "$rest" "$@"'
reqarg="$op1arg;"'test -z "${1+1}" &&echo opt -$op requires arg. &&echo "$USAGE" &&exit'
args= do_help= opt_v=0
while [ -n "${1-}" ];do
    if expr "x${1-}" : 'x-' >/dev/null;then
        op=`expr "x$1" : 'x-\(.*\)'`; shift   # done with $1
        leq=`expr "x$op" : 'x-[^=]*\(=\)'` lev=`expr "x$op" : 'x-[^=]*=\(.*\)'`
        test -n "$leq"&&eval "set -- \"\$lev\" \"\$@\""&&op=`expr "x$op" : 'x\([^=]*\)'`
        case "$op" in
        \?*|h*)    eval $op1chr; do_help=1;;
        v*)        eval $op1chr; opt_v=`expr $opt_v + 1`;;
        x*)        eval $op1chr; set -x;;
        -run-demo) opt_run_demo=--run-demo;;
	-debug)    opt_debug=--debug;;
        -not-lbne-raw-data-developer)  opt_http_download_lbne_raw_data=--not-lbne-raw-data-developer;;
        -not-artdaq-developer)         opt_http_download_artdaq=--not-artdaq-developer;;
        *)         echo "Unknown option -$op"; do_help=1;;
        esac
    else
        aa=`echo "$1" | sed -e"s/'/'\"'\"'/g"` args="$args '$aa'"; shift
    fi
done
eval "set -- $args \"\$@\""; unset args aa

test -n "${do_help-}" -o $# -ne 2 && echo "$USAGE" && exit

test -d $1 || { echo "products directory ($1) not found"; exit 1; }
products_dir=`cd "$1" >/dev/null;pwd`
lbne_artdaq_dir=`cd "$2" >/dev/null;pwd`
demo_dir=`dirname "$lbne_artdaq_dir"`

export CETPKG_INSTALL=$products_dir
export CETPKG_J=16

test -d "$demo_dir/build_lbne-artdaq" || mkdir "$demo_dir/build_lbne-artdaq" 

if [[ -n "${opt_debug:-}" ]];then
    build_arg="d"
else
    build_arg="p"
fi


REPO_PREFIX=http://cdcvs.fnal.gov/projects
#REPO_PREFIX=ssh://p-artdaq@cdcvs.fnal.gov/cvs/projects # p-artdaq can be used to access artdaq-demo

function install_package {
    local packagename=$1
    local commit_tag=$2

    # Get rid of the first two positional arguments now that they're stored in named variables
    shift;
    shift;

    test -d "$demo_dir/build_$packagename" || mkdir "$demo_dir/build_$packagename"    

    test -d ${packagename} || git clone $REPO_PREFIX/$packagename
    cd $packagename
    git fetch origin
    git checkout $commit_tag
    cd ../build_$packagename

    echo IN $PWD: about to . ../$packagename/ups/setup_for_development

# JCF, Sep-8-2015

# Added "unsetup cetbuildtools" as it's fine for a given package to
# use a version of cetbuildtools not used by the others

    unsetup cetbuildtools  
    . ../$packagename/ups/setup_for_development -${build_arg} $@
    echo FINISHED ../$packagename/ups/setup_for_development
    buildtool ${opt_clean+-c} -i -t
    cd ..
}

. $products_dir/setup

install_package artdaq-core v1_04_17 e7 s15
install_package lbne-raw-data feature/penn_board_reader e7 s15

install_package artdaq v1_12_12a e7 s15 eth

setup_qualifier="e7"


if [[ "$HOSTNAME" != "lbne35t-gateway01.fnal.gov" ]] ; then
    setup_cmd="source $products_dir/setup"
else
    setup_cmd="source /data/lbnedaq/products/setup; source $products_dir/setup"
fi

cd $demo_dir >/dev/null
if [[ ! -e ./setupLBNEARTDAQ ]]; then
    cat >setupLBNEARTDAQ <<-EOF
	echo # This script is intended to be sourced.

	sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running the lbne-artdaq.'; exit; }" || exit

	$setup_cmd

	export CETPKG_INSTALL=$products_dir
	export CETPKG_J=16
	#export LBNEARTDAQ_BASE_PORT=52200
	export DAQ_INDATA_PATH=$lbne_artdaq_dir/test/Generators:$lbne_artdaq_dir/inputData

	export LBNEARTDAQ_BUILD="$demo_dir/build_lbne-artdaq"
	export LBNEARTDAQ_REPO="$lbne_artdaq_dir"
	export FHICL_FILE_PATH=.:\$LBNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH

	echo changing directory to \$LBNEARTDAQ_BUILD
	cd \$LBNEARTDAQ_BUILD  # note: next line adjusts PATH based one cwd
	. \$LBNEARTDAQ_REPO/ups/setup_for_development -${build_arg} $setup_qualifier

# JCF, 11/25/14
# Make it easy for users to take a quick look at their output file via "rawEventDump"

alias rawEventDump="art -c \$LBNEARTDAQ_REPO/tools/fcl/rawEventDump.fcl "

	EOF
    #
fi


echo "Building lbne-artdaq..."
cd $LBNEARTDAQ_BUILD
unsetup cetbuildtools  # See comment above for why this is done
. $demo_dir/setupLBNEARTDAQ
buildtool -t

echo "Installation and build complete; please see https://cdcvs.fnal.gov/redmine/projects/lbne-artdaq/wiki/Running_a_sample_lbne-artdaq_system for instructions on how to run"

if [ -n "${opt_run_demo-}" ];then
    echo doing the demo

    $lbne_artdaq_dir/tools/xt_cmd.sh $demo_dir --geom '132x33 -sl 2500' \
        -c '. ./setupLBNEARTDAQ' \
        -c start2x2x2System.sh
    sleep 2

    $lbne_artdaq_dir/tools/xt_cmd.sh $demo_dir --geom 132 \
        -c '. ./setupLBNEARTDAQ' \
        -c ':,sleep 10' \
        -c 'manage2x2x2System.sh -m on init' \
        -c ':,sleep 5' \
        -c 'manage2x2x2System.sh -N 101 start' \
        -c ':,sleep 90' \
        -c 'manage2x2x2System.sh stop' \
        -c ':,sleep 5' \
        -c 'manage2x2x2System.sh shutdown' \
        -c ': For additional commands, see output from: manage2x2x2System.sh --help' \
        -c ':: manage2x2x2System.sh --help' \
        -c ':: manage2x2x2System.sh exit'
fi
