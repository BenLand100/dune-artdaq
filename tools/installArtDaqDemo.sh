#!/bin/bash

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


products_dir=`cd "$1" >/dev/null;pwd`
lbne_artdaq_dir=`cd "$2" >/dev/null;pwd`
demo_dir=`dirname "$lbne_artdaq_dir"`

export CETPKG_INSTALL=$products_dir
export CETPKG_J=16

test -d "$demo_dir/build_artdaq-core" || mkdir "$demo_dir/build_artdaq-core" 
test -d "$demo_dir/build_lbne-raw-data"      || mkdir "$demo_dir/build_lbne-raw-data" 
test -d "$demo_dir/build_artdaq"      || mkdir "$demo_dir/build_artdaq"  
test -d "$demo_dir/build_lbne-artdaq" || mkdir "$demo_dir/build_lbne-artdaq" 

if [[ -n "${opt_debug:-}" ]];then
    build_arg="d"
else
    build_arg="p"
fi

# Commit 52d6e7b4527dce8a86b7bcaf5970d45013373b89, from 9/15/14,
# updates artdaq core v1_04_00 s.t. it includes the BuildInfo template

test -d artdaq-core || git clone http://cdcvs.fnal.gov/projects/artdaq-core
cd artdaq-core
git fetch origin
git checkout 52d6e7b4527dce8a86b7bcaf5970d45013373b89
cd ../build_artdaq-core
echo IN $PWD: about to . ../artdaq-core/ups/setup_for_development
. $products_dir/setup
. ../artdaq-core/ups/setup_for_development -${build_arg} e5 s3
echo FINISHED ../artdaq-core/ups/setup_for_development
buildtool -i
cd ..

# lbne-raw-data commit e9dbaa11d248033fbf75f22e162a3ab0eaa0dfb6, from
# 9/16/14, compiles with the e5:s3 option (against artdaq-core
# v1_04_00, etc.), and adds a traits class supplying build info

if [[ -n "${opt_http_download_lbne_raw_data:-}" ]];then
    test -d lbne-raw-data || git clone http://cdcvs.fnal.gov/projects/lbne-raw-data
else
    test -d lbne-raw-data || git clone ssh://p-lbne-raw-data@cdcvs.fnal.gov/cvs/projects/lbne-raw-data
fi
cd lbne-raw-data
git fetch origin
git checkout de9dbaa11d248033fbf75f22e162a3ab0eaa0dfb6
cd ../build_lbne-raw-data
echo IN $PWD: about to . ../lbne-raw-data/ups/setup_for_development
. $products_dir/setup
. ../lbne-raw-data/ups/setup_for_development -${build_arg} e5 s3
echo FINISHED ../lbne-raw-data/ups/setup_for_development
buildtool -i
cd ..

# artdaq commit f0f0c5eb950f5a53e06aee564975357c4bc5da7e, from
# 9/16/14, includes both the merging of the buildinfo branch and the
# timestamps branch

test -d artdaq || git clone http://cdcvs.fnal.gov/projects/artdaq
cd artdaq
git fetch origin
git checkout f0f0c5eb950f5a53e06aee564975357c4bc5da7e
cd ../build_artdaq
echo IN $PWD: about to . ../artdaq/ups/setup_for_development
. $products_dir/setup
. ../artdaq/ups/setup_for_development -${build_arg} e5 s3 eth
echo FINISHED ../artdaq/ups/setup_for_development
buildtool -i
cd ..






cd $demo_dir >/dev/null
if [[ ! -e ./setupLBNEARTDAQ ]]; then
    cat >setupLBNEARTDAQ <<-EOF
	echo # This script is intended to be sourced.

	sh -c "[ \`ps \$\$ | grep bash | wc -l\` -gt 0 ] || { echo 'Please switch to the bash shell before running the lbne-artdaq.'; exit; }" || exit

	source $products_dir/setup

	export CETPKG_INSTALL=$products_dir
	export CETPKG_J=16
	#export LBNEARTDAQ_BASE_PORT=52200
	export DAQ_INDATA_PATH=$lbne_artdaq_dir/test/Generators:$lbne_artdaq_dir/inputData

	export LBNEARTDAQ_BUILD="$demo_dir/build_lbne-artdaq"
	export LBNEARTDAQ_REPO="$lbne_artdaq_dir"
	export FHICL_FILE_PATH=.:\$LBNEARTDAQ_REPO/tools/fcl:\$FHICL_FILE_PATH

	echo changing directory to \$LBNEARTDAQ_BUILD
	cd \$LBNEARTDAQ_BUILD  # note: next line adjusts PATH based one cwd
	. \$LBNEARTDAQ_REPO/ups/setup_for_development -${build_arg} e5 s3 eth

	EOF
    #
fi


echo "Building lbne-artdaq..."
cd $LBNEARTDAQ_BUILD
. $demo_dir/setupLBNEARTDAQ
buildtool

echo "Installation and build complete; please see https://cdcvs.fnal.gov/redmine/projects/lbne-artdaq/wiki/Running_a_sample_lbne-artdaq_system for instructions on how to run"

if [ -n "${opt_run_demo-}" ];then
    echo doing the demo

    $lbne_artdaq_dir/tools/xt_cmd.sh $demo_dir --geom 132x33 \
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
