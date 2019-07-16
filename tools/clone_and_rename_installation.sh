#!/bin/env bash

if [[ "$#" != "2"  ]]; then
    echo "Usage: $0 <path to installation to copy from> <path to installation to copy to>" >&2
    exit 1
fi


cat<<EOF

JCF, Jun-6-2019

This script has been deprecated, as we're trying to move to a model in
which developers set up a new development area by installing code
which has been committed to the central repository at Fermilab. If
you're cloning because you want to get your hands on someone else's
code, ask that person to commit the code to the central repository
first it this hasn't happened already, and then pull it from the
central repository after installing your new development area.

Instructions on how to get set up so you can push code to the central
repository can be found at
https://twiki.cern.ch/twiki/bin/view/CENF/Deployment .

Instructions how how to set up a new area can be found at
https://twiki.cern.ch/twiki/bin/view/CENF/Installation .

EOF

exit 1


orig_installation=$1
new_installation=$2

# Standardize the formatting of the directory names - i.e., make sure neither has a trailing "/"
orig_installation=$(echo $orig_installation | awk '{ gsub("/$", ""); print }' )
new_installation=$(echo $new_installation | awk '{ gsub("/$", ""); print }' )

processors=32

if [[ ! -e $orig_installation ]]; then
    echo "Original dune-artdaq installation $orig_installation doesn't appear to exist; exiting..." >&2
    exit 2
fi

if [[ -e $new_installation ]]; then
    echo "Requested new installation area $new_installation already appears to exist; exiting..." >&2
    exit 3
fi

if [[ ! -e $orig_installation/setupDUNEARTDAQ_forBuilding ]]; then
    echo "Unable to find expected file $orig_installation/setupDUNEARTDAQ_forBuilding in original dune-artdaq installation $orig_installation; exiting..." >&2
    exit 5
    
fi

nfs_base=/nfs/sw/work_dirs
local_base=/scratch

if [[ "$orig_installation" =~ ^$nfs_base ]]; then
    is_nfs_area=true
elif [[ "$orig_installation" =~ ^$local_base ]]; then
    is_nfs_area=false
else

    cat<<EOF >&2

This script can't clone dune-artdaq development areas that aren't
either in a subdirectory of $nfs_base or $local_base. Exiting...

EOF
    exit 1
fi

if ! $is_nfs_area ; then
    orig_installation_for_running=$( echo $orig_installation | sed -r 's#'$local_base'#'$nfs_base'#' )
    new_installation_for_running=$( echo $new_installation | sed -r 's#'$local_base'#'$nfs_base'#' )

    if [[ -e $orig_installation_for_running ]]; then
	mkdir $new_installation_for_running

	if [[ "$?" != 0 ]]; then
	    echo "There was a problem attempting to make directory $new_installation_for_running (perhaps it already existed?); exiting..." >&2
	    exit 1
	fi

	for script in setupDUNEARTDAQ_forTRACE setupDUNEARTDAQ_forInhibitMaster setupDUNEARTDAQ_forRunning; do

	    if [[ -e $orig_installation_for_running/$script ]]; then
		cp $orig_installation_for_running/$script $new_installation_for_running
	    else
		echo "Unable to find expected script $orig_installation_for_running/$script, exiting..." >&2
		exit 1
	    fi
	done
    else
	cat<<EOF

Expected there to be a directory on the NFS disk,
$orig_installation_for_running, used for running the code built in
$orig_installation. No such directory was found. Exiting...

EOF
	exit 1
    fi
fi

num_local_products_dirs=$( find $orig_installation -maxdepth 1 -type d -regex ".*localProducts.*prof\|debug" | wc -l )

if (( $num_local_products_dirs == 1 )); then
    local_products_dir=$( find $orig_installation -maxdepth 1 -type d -regex ".*localProducts.*prof\|debug" )

    project=$( echo $local_products_dir | sed -r 's/.*localProducts_(.*)_v[0-9]+_.*/\1/' )
    version=$( echo $local_products_dir | sed -r 's/.*localProducts_.*_(v[0-9]+.*)_e[0-9]+.*/\1/' )
    qualifiers=$(  echo $local_products_dir | sed -r 's/.*localProducts_.*_.*_(e[0-9]+)_prof|debug/\1/'  )
    buildtype=$( echo $local_products_dir | sed -r 's/.*localProducts_.*_(prof|debug)/\1/' )

    echo "Project: $project"
    echo "Version: $version"
    echo "Qualifiers: $qualifiers"
    echo "Build type: $buildtype"

else
    echo "Expected to find one local products directory in ${orig_installation}; found $num_local_products_dirs instead. Exiting..." >&2
    exit 5
fi

orig_installation_size=$( du $orig_installation -s | awk '{print $1}' )

cat<<EOF

Will now copy $orig_installation to $new_installation; size of
$orig_installation is $orig_installation_size kilobytes

EOF

begintime=$( date +%s )
cp -rp $orig_installation $new_installation
endtime=$( date +%s )
echo "Copy took "$(( endtime - begintime ))" seconds; will now attempt to rebuild new installation"

cd $new_installation

if $is_nfs_area; then
orig_installation_subdirs=$( echo $orig_installation | sed -r 's#'$nfs_base'/(.*)#\1#' )
new_installation_subdirs=$( echo $new_installation | sed -r 's#'$nfs_base'/(.*)#\1#' )
else
orig_installation_subdirs=$( echo $orig_installation | sed -r 's#'$local_base'/(.*)#\1#' )
new_installation_subdirs=$( echo $new_installation | sed -r 's#'$local_base'/(.*)#\1#' )
fi

if $is_nfs_area; then
    scripts_to_edit="$new_installation/setupDUNEARTDAQ_forBuilding $new_installation/setupDUNEARTDAQ_forRunning  $new_installation/setupDUNEARTDAQ_forTRACE  $new_installation/setupDUNEARTDAQ_forInhibitMaster"
else
    scripts_to_edit="$new_installation/setupDUNEARTDAQ_forBuilding $new_installation_for_running/setupDUNEARTDAQ_forRunning  $new_installation_for_running/setupDUNEARTDAQ_forTRACE  $new_installation_for_running/setupDUNEARTDAQ_forInhibitMaster"
fi


for script in $scripts_to_edit; do
    echo     sed -r -i 's#trace_.*#trace_'$( echo $new_installation_subdirs | sed -r 's#^/##;s#/#_#g')'#' $script
    echo     sed -r -i 's#'$orig_installation_subdirs'/#'$new_installation_subdirs'/#g' $script
    sed -r -i 's#trace_.*#trace_'$( echo $new_installation_subdirs | sed -r 's#^/##;s#/#_#g')'#' $script
    sed -r -i 's#'$orig_installation_subdirs'/#'$new_installation_subdirs'/#g' $script
done

rm -rf $new_installation/localProducts_*
rm -rf $new_installation/build_slf7.x86_64

if ! [[ -n $( diff $orig_installation/setupDUNEARTDAQ_forBuilding $new_installation/setupDUNEARTDAQ_forBuilding ) ]] ; then
    echo "$orig_installation/setupDUNEARTDAQ_forBuilding and $new_installation/setupDUNEARTDAQ_forBuilding are identical, please contact John Freeman (jcfreeman2@gmail.com) if you have any questions. Exiting..." >&2
    exit 6
fi

source /nfs/sw/artdaq/products/setup

setup mrb
setup git 
setup gitflow

export MRB_PROJECT=$project

mrb newDev -f -v $version  -q $qualifiers:$buildtype

if [[ "$?" != "0" ]]; then
    echo "There was a problem with mrb newDev" >&2
    exit 7
fi

no_mrb_script=localProducts_${project}_${version}_${qualifiers}_${buildtype}/setupWithoutMRB

if [[ -e $orig_installation/$no_mrb_script ]]; then
    if $is_nfs_area; then
	cp $orig_installation/$no_mrb_script $new_installation/$no_mrb_script
    else
	cp -rp $PWD/localProducts_${project}_${version}_${qualifiers}_${buildtype} $new_installation_for_running
	cp $orig_installation/$no_mrb_script $new_installation_for_running/$no_mrb_script
    fi
fi

source /nfs/sw/artdaq/products/setup                                      
source /nfs/sw/artdaq/products_dev/setup    
source $PWD/localProducts_${project}_${version}_${qualifiers}_${buildtype}/setup

if [[ "$?" != "0" ]]; then
    echo "There was a problem source-ing $PWD/localProducts_${project}_${version}_${qualifiers}_${buildtype}/setup" >&2
    exit 8
fi

echo "About to call mrb z...."

mrb z
mrbsetenv
mrb install -j$processors

echo "FAILURE ABOVE IS EXPECTED; WILL PERFORM REBUILD ATFER SOURCE OF setupDUNEARTDAQ_forBuilding..."

. setupDUNEARTDAQ_forBuilding
mrb install -j$processors


# Can remove this as soon as I figure out how to copy softlinks correctly when doing a recursive directory copy...
if $is_nfs_area; then
    rm -f $new_installation/setupDUNEARTDAQ
    ln -s $new_installation/setupDUNEARTDAQ_forRunning $new_installation/setupDUNEARTDAQ 
else
    rm -f $new_installation_for_running/setupDUNEARTDAQ
    ln -s $new_installation_for_running/setupDUNEARTDAQ_forRunning $new_installation_for_running/setupDUNEARTDAQ
fi

