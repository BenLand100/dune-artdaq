#! /bin/env bash
#  This file (lbne-artdaq-quickstart.sh) was created by Ron Rechenmacher <ron@fnal.gov> on
#  Jan  7, 2014. "TERMS AND CONDITIONS" governing this file are in the README
#  or COPYING file. If you do not have such a file, one can be obtained by
#  contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
#  $RCSfile: .emacs.gnu,v $
rev='$Revision: 1.20 $$Date: 2010/02/18 13:20:16 $'
#
#  This script is based on the original createArtDaqDemo.sh script created by
#  Kurt and modified by John.

#
# This script will:
#      1.  get (if not already gotten) lbne-artdaq and all support products
#          Note: this whole demo takes approx. X Megabytes of disk space
#      2a. possibly build the artdaq dependent product
#      2b. build (via cmake),
#  and 3.  start the lbne-artdaq system
#


# program (default) parameters
root=
tag=

env_opts_var=`basename $0 | sed 's/\.sh$//' | tr 'a-z-' 'A-Z_'`_OPTS
USAGE="\
   usage: `basename $0` [options] [demo_root]
examples: `basename $0` .
          `basename $0` --run-demo
If the \"demo_root\" optional parameter is not supplied, the user will be
prompted for this location.
--run-demo                       runs the demo
-f                               force download
--skip-check                     skip the free diskspace check
--debug                          use a debug build
--not-lbne-raw-data-developer    checkout lbne-raw-data using read-only http
--not-artdaq-developer           checkout artdaq using read-only http
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
        \?*|h*)     eval $op1chr; do_help=1;;
        v*)         eval $op1chr; opt_v=`expr $opt_v + 1`;;
        x*)         eval $op1chr; set -x;;
        f*)         eval $op1chr; opt_force=1;;
        t*|-tag)    eval $reqarg; tag=$1;    shift;;
        -skip-check)opt_skip_check=1;;
        -run-demo)  opt_run_demo=--run-demo;;
	-debug)     opt_debug=--debug;;
	-not-lbne-raw-data-developer)  opt_http_download_lbne_raw_data=--not-lbne-raw-data-developer;;
	-not-artdaq-developer)         opt_http_download_artdaq=--not-artdaq-developer;;
        *)          echo "Unknown option -$op"; do_help=1;;
        esac
    else
        aa=`echo "$1" | sed -e"s/'/'\"'\"'/g"` args="$args '$aa'"; shift
    fi
done
eval "set -- $args \"\$@\""; unset args aa
set -u   # complain about uninitialed shell variables - helps development


test -n "${do_help-}" -o $# -ge 2 && echo "$USAGE" && exit
test $# -eq 1 && root=$1

#check that $0 is in a git repo
tools_path=`dirname $0`
tools_path=`cd "$tools_path" >/dev/null;pwd`
tools_dir=`basename $tools_path`
git_working_path=`dirname $tools_path`
cd "$git_working_path" >/dev/null
git_working_path=$PWD
git_status=`git status 2>/dev/null`
git_sts=$?
if [ $git_sts -ne 0 -o $tools_dir != tools ];then
    echo problem with git or quick-start.sh script is not from/in a git repository
    exit 1
fi

branch=`git branch | sed -ne '/^\*/{s/^\* *//;p;q}'`
echo the current branch is $branch
if [ "$branch" != '(no branch)' ];then
    test -z "$tag" && tag=`git tag -l 'v[0-9]*' | tail -n1`
    if [ "$tag" != "$branch" ];then
        echo "checking out tag $tag"
        git checkout $tag
    fi
fi

# JCF, 9/24/14

# Now that we've checked out the lbne-artdaq version we want, make
# sure we know what qualifier is meant to be passed to the
# installArtDaqDemo.sh scripts below


version=`grep -E "^\s*artdaq\s+" $git_working_path/ups/product_deps | awk '{print $2}'`
equalifier="e999"
squalifier="s999"

if [[ "$version" == "v1_13_02" ]]; then
    equalifier="e10"
    squalifier="s35"
else
    echo "Unsupported artdaq version" >&2
    exit 1
fi 

equalifier_xcheck=$( grep "^[^#]*defaultqual" $git_working_path/ups/product_deps | awk '{print $2}' )
if [[ "${equalifier_xcheck}" != "$equalifier" ]]; then
    echo "Mismatch between calculated e-qualifier ($equalifier) and default e-qualifier (${equalifier_xcheck})" >&2
    exit 1
fi


vecho() { test $opt_v -gt 0 && echo "$@"; }
starttime=`date`

if [ -z "$root" ];then
    root=`dirname $git_working_path`
    echo the root is $root
fi
test -d "$root" || mkdir -p "$root"
cd $root

# JCF, 1/23/15

# Save all output from this script (stdout + stderr) in a file with a
# name that looks like "quick-start.sh_Fri_Jan_16_13:58:27.script" as
# well as all stderr in a file with a name that looks like
# "quick-start.sh_Fri_Jan_16_13:58:27_stderr.script"

alloutput_file=$( date | awk -v "SCRIPTNAME=$(basename $0)" '{print SCRIPTNAME"_"$1"_"$2"_"$3"_"$4".script"}' )

stderr_file=$( date | awk -v "SCRIPTNAME=$(basename $0)" '{print SCRIPTNAME"_"$1"_"$2"_"$3"_"$4"_stderr.script"}' )

mkdir -p "$root/log"
exec  > >(tee "$root/log/$alloutput_file")
exec 2> >(tee "$root/log/$stderr_file")

echo
echo "If you don't have C++ bindings for ZeroMQ installed on your system, see note at top of https://cdcvs.fnal.gov/redmine/projects/lbne-artdaq/wiki/Installing_and_building_the_lbne-artdaq_code for what to do"
echo 'To determine whether you have ZeroMQ, use the command "locate zmq.hpp" or contact jcfree@fnal.gov to find this out'
echo

free_disk_needed=5

free_disk_G=`df -B1G . | awk '/[0-9]%/{print$(NF-2)}'`
if [ -z "${opt_skip_check-}" -a "$free_disk_G" -lt $free_disk_needed ];then
    echo "Error: insufficient free disk space ($free_disk_G). Min. require = $free_disk_needed"
    echo "Use the --skip-check option to skip free space check."
    exit 1
fi


if [ ! -x $git_working_path/tools/installArtDaqDemo.sh ];then
    echo error: missing tools/installArtDaqDemo.sh
    exit 1
fi

if [[ -n "${opt_debug:-}" ]] ; then
    build_type="debug"
else
    build_type="prof"
fi

os=""

echo "Are you sure you want to download and install the lbne-artdaq dependent products in `pwd`? [y/n]"
read response
if [[ "$response" != "y" ]]; then
    echo "Aborting..."
    exit
fi
test -d products || mkdir products
test -d download || mkdir download

cd download

wget http://scisoft.fnal.gov/scisoft/bundles/tools/pullProducts
chmod +x pullProducts
    
echo "Cloning cetpkgsupport to determine current OS"
git clone http://cdcvs.fnal.gov/projects/cetpkgsupport
os=`./cetpkgsupport/bin/get-directory-name os`

qualifiers="${squalifier}-${equalifier}"
cmd="./pullProducts ../products ${os} artdaq-${version} $qualifiers $build_type"
echo "Running $cmd"
$cmd

if [ $? -ne 0 ]; then
    echo "Error in pullProducts. Please go to http://scisoft.fnal.gov/scisoft/bundles/artdaq/${version}/manifest and make sure that a manifest for the specified qualifiers ($qualifiers) exists."
    exit 1
fi

cd ..

# JCF, Jul-13-2016
# The gallery package is not handled by pullProducts; hence the manual download
# v1_03_02 is needed for the set of packages which include art v2_01_02 and canvas v1_04_02

gallery_url=http://scisoft.fnal.gov/scisoft/packages/gallery/v1_03_02/gallery-1.03.02-${os}-x86_64-${equalifier}-${build_type}.tar.bz2

cd products

for url in $gallery_url ; do 
    curl -O $url
    tar xjf $( basename $url )

    if [ $? -ne 0 ]; then
	echo "Problem downloading and/or extracting archive file $url" >&2
	exit 1
    fi

done



cd ..

$git_working_path/tools/installArtDaqDemo.sh products $git_working_path ${opt_run_demo-} ${opt_debug-} ${opt_http_download_lbne_raw_data-} ${opt_http_download_artdaq-}

endtime=`date`

echo $starttime
echo $endtime



