#!/bin/bash

env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
  usage: `basename $0` [options] <demo_products_dir/> <lbne-artdaq/>
example: `basename $0` products lbne-artdaq --run-demo
<demo_products>    where products were installed (products/)
<lbne-artdaq_root> directory where lbne-artdaq was cloned into.
--run-demo   runs the demo
Currently this script will clone (if not already cloned) artdaq
along side of the lbne-artdaq dir.
Also it will create, if not already created, build directories
for artdaq and lbne-artdaq.
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

test -d "$demo_dir/build_artdaq"      || mkdir "$demo_dir/build_artdaq"  # This is where we will build artdaq
test -d "$demo_dir/build_lbne-artdaq" || mkdir "$demo_dir/build_lbne-artdaq"  # This is where we will build lbne-artdaq


# Get artdaq from central git repository
test -d artdaq || git clone http://cdcvs.fnal.gov/projects/artdaq
cd artdaq
git fetch origin
git checkout v1_05_06
cd ../build_artdaq
echo IN $PWD: about to . ../artdaq/ups/setup_for_development
. $products_dir/setup
. ../artdaq/ups/setup_for_development -p e4 eth
echo FINISHED ../artdaq/ups/setup_for_development
export CETPKG_INSTALL=$products_dir
export CETPKG_J=16
buildtool -i

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
	. \$LBNEARTDAQ_REPO/ups/setup_for_development -p e4 eth

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
