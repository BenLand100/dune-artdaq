#!/bin/env bash

if [[ "$#" != "2"  ]]; then
    echo "Usage: $0 <path to installation to copy from> <path to installation to copy to>" >&2
    exit 1
fi

orig_installation=$1
new_installation=$2

# Standardize the formatting of the directory names - i.e., make sure neither has a trailing "/"
orig_installation=$(echo $orig_installation | awk '{ gsub("/$", ""); print }' )
new_installation=$(echo $new_installation | awk '{ gsub("/$", ""); print }' )


node_number=$( echo $HOSTNAME | awk -F- '{print $NF}' )

processors=32


if [[ ! -e $orig_installation ]]; then
    echo "Original dune-artdaq installation $orig_installation doesn't appear to exist; exiting..." >&2
    exit 2
fi

if [[ -e $new_installation ]]; then
    echo "Requested new installation area $new_installation already appears to exist; exiting..." >&2
    exit 3
fi

if [[ ! $new_installation =~ ^/nfs/sw/work_dirs ]]; then
    echo "Requested new installation area $new_installation should be located in /nfs/sw/work_dirs; exiting..." >&2
    exit 4
fi

if [[ ! -e $orig_installation/setupDUNEARTDAQ_forBuilding ]]; then
    echo "Unable to find expected file $orig_installation/setupDUNEARTDAQ_forBuilding in original dune-artdaq installation $orig_installation; exiting..." >&2
    exit 5
    
fi

num_local_products_dirs=$( find $orig_installation -maxdepth 1 -type d -regex ".*localProducts.*prof\|debug" | wc -l )

if (( $num_local_products_dirs == 1 )); then
    local_products_dir=$( find $orig_installation -maxdepth 1 -type d -regex ".*localProducts.*prof\|debug" )

    project=$( echo $local_products_dir | sed -r 's/.*localProducts_(.*)_v[0-9]+_.*/\1/' )
    version=$( echo $local_products_dir | sed -r 's/.*localProducts_.*_(v[0-9]+_[0-9]+_[^_]+).*/\1/' )
    qualifiers=$(  echo $local_products_dir | sed -r 's/.*localProducts_.*_v[0-9]+_[0-9]+_[^_]+_(.*)_prof|debug/\1/'  )
    buildtype=$( echo $local_products_dir | sed -r 's/.*localProducts_.*_v[0-9]+_[0-9]+_[^_]+_.*_(prof|debug)/\1/' )

    echo "Project: $project"
    echo "Version: $version"
    echo "Qualifiers: $qualifiers"
    echo "Build type: $buildtype"

else
    echo "Expected to find 1 local products directory in ${orig_installation}; found $num_local_products_dirs instead. Exiting..." >&2
    exit 5
fi

orig_installation_size=$( du $orig_installation -s | awk '{print $1}' )

cat<<EOF

Will now copy $orig_installation to $new_installation; size of
$orig_installation is $orig_installation_size bytes

EOF

begintime=$( date +%s )
cp -rp $orig_installation $new_installation
endtime=$( date +%s )
echo "Copy took "$(( endtime - begintime ))" seconds; will now attempt to rebuild new installation"

cd $new_installation

orig_installation_subdir=$( basename $orig_installation )
new_installation_subdir=$( basename $new_installation )
sed -r -i 's#trace_'$orig_installation_subdir'#trace_'$new_installation_subdir'#g' $new_installation/setupDUNEARTDAQ*

sed -r -i 's#'$orig_installation'/#'$new_installation'/#g' $new_installation/setupDUNEARTDAQ*

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
    cp $orig_installation/$no_mrb_script $new_installation/$no_mrb_script
fi

source $PWD/localProducts_${project}_${version}_${qualifiers}_${buildtype}/setup

if [[ "$?" != "0" ]]; then
    echo "There was a problem source-ing $PWD/localProducts_${project}_${version}_${qualifiers}_${buildtype}/setup" >&2
    exit 8
fi

# Can remove this as soon as I figure out how to copy soflinks correctly when doing a recursive directory copy...
if [[ -e setupDUNEARTDAQ_forRunning ]]; then
    rm -f setupDUNEARTDAQ
    ln -s setupDUNEARTDAQ_forRunning setupDUNEARTDAQ 
fi

echo "About to call mrb z...."

mrb z
mrbsetenv
mrb install -j$processors

echo "FAILURE ABOVE IS EXPECTED; WILL PERFORM REBUILD ATFER SOURCE OF setupDUNEARTDAQ_forBuilding..."

. setupDUNEARTDAQ_forBuilding
mrb install -j$processors


