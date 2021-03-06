#!/bin/env bash

if [[ "$#" != "3" ]]; then
    echo
    echo "Usage: "$( basename $0)" <package> <descriptive token -- use \"\" if you don't want one> <run # you tested with>"
    echo
    exit 0
fi

package=$1
descriptive_token=$2
run_tested=$3

if [[ -n $MRB_TOP ]]; then

    cat<<EOF >&2

    Environment variable MRB_TOP is already defined ($MRB_TOP); this
    suggests that you've already set up a build environment, which can
    interfere with the running of this script. Please log into a clean
    shell and try again. Exiting...

EOF
    exit 1
fi

if [[ -n $( echo $package | grep "-" ) ]]; then
    echo "You provided the package name as \"$package\"; for consistency's sake please replace the dashes \"-\" with underscores \"_\". Exiting..." >&2
    exit 1
fi

if ! [[ $run_tested =~ ^[0-9]+$ ]]; then
    echo "Run number provided, \"$run_tested\", doesn't appear to be an integer. Exiting..." >&2
    exit 1
fi

run_records_dir=/nfs/sw/artdaq/run_records

if ! [[ -d $run_records_dir/$run_tested ]]; then
    echo "No record found of run $run_tested in ${run_records_dir}; exiting..." >&2
    exit 1
fi

development_area_from_run=$( sed -r -n 's/^\s*DAQ\s+setup\s+script\s*:\s*(.*)\/setupDUNEARTDAQ.*/\1/p' $run_records_dir/$run_tested/boot.txt )
if [[ -n $development_area_from_run ]]; then
    if [[ "$PWD" != "$development_area_from_run" ]]; then

	cat<<EOF >&2

You don't appear to be in the dune-artdaq development directory
($development_area_from_run) used in the run you provided ($run_tested); 
right now you're in $PWD. Exiting...

EOF

	exit 1
    fi
else
    echo "There was a problem determining the development area from run $run_tested's boot file; exiting..." >&2
    exit 1
fi

packagedir=$( echo $package | sed -r 's/-/_/' )  # In case the logic above ever changes

sourceme_for_build="./setupDUNEARTDAQ_forBuilding"

if [[ ! -e $sourceme_for_build ]]; then
    echo "Unable to find file \"$sourceme_for_build\" in $PWD. You need to be in a dune-artdaq development area to run this. Exiting..." >&2
    exit 1
fi

if [[ ! -e ./srcs/$packagedir ]]; then
    echo "Expected there to be a git repo ./srcs/$packagedir available, but couldn't find one. Exiting..." >&2
    exit 1
fi

cd ./srcs/$packagedir

echo "Calling \"git fetch origin\" from $PWD..."
git fetch origin

if [[ "$?" != "0" ]]; then
    cat<<EOF >&2

There was a problem calling git fetch origin in ./srcs/$packagedir;
note that you need to have ownership of the repo for this script to
work. Exiting...

EOF

    exit 1
fi
echo "Done with \"git fetch origin\" call"
echo
echo "Calling \"git fetch --tags\" from $PWD"
git fetch --tags

if [[ "$?" != "0" ]]; then
    echo "There was a problem calling git fetch --tags in ./srcs/$packagedir. Exiting..." >&2
    exit 1
fi
echo "Done with \"git fetch --tags\" call"

# I apologize on behalf of git for there not to be a simple way to JUST get the branch you're on
branchname=$( git branch | sed -r -n 's/^\s*\*\s*(.*)/\1/p' )
branchname=$( echo $branchname | sed -r 's/\//_/g'  )


echo "Checking to make sure the remote $branchname branch (if it exists) isn't ahead of the local $branchname"

if [[ -n $( git branch -a | grep "origin/$branchname" ) ]]; then

    remotehead=$( git log origin/$branchname -1 | sed -r -n 's/^commit\s+(.*)/\1/p' )

    if [[ -z $( git log $branchname | grep $remotehead ) ]]; then
	echo "Error: current HEAD of origin/$branchname is not found in the local $branchname in ./srcs/$packagedir. This suggests that you won't be able to push your code as-is to the central repo"
	echo
	echo "HEAD of origin/$branchname:"
	git log -1 origin/$branchname
	echo
	echo "Exiting..."
	exit 1
    fi
fi
echo "Done checking that remote $branchname branch (if it exists) isn't ahead of the local $branchname"

year_in_greenwich=$( date --utc +%Y )
month_in_greenwich=$( date --utc +%m )
day_in_greenwich=$(  date --utc +%d )

if [[ "$descriptive_token" != "" ]]; then
    tagname=v${year_in_greenwich}_${month_in_greenwich}_${day_in_greenwich}_${branchname}_${descriptive_token}
else
    tagname=v${year_in_greenwich}_${month_in_greenwich}_${day_in_greenwich}_${branchname}
fi

echo "Tag is \"$tagname\""

cd ../..

if [[ $packagedir == "dune_artdaq" ]]; then
    dependent_packages="artdaq dune_raw_data artdaq_core artdaq_utilities"
elif [[ $packagedir == "dune_raw_data" ]]; then
    dependent_packages="artdaq_core"
elif [[  $packagedir == "artdaq" ]]; then
    dependent_packages="artdaq_core artdaq_utilities" 
elif [[ $packagedir == "artdaq_mpich_plugin" ]]; then
    dependent_packages="artdaq artdaq_core artdaq_utilities"
elif [[ $packagedir == "artdaq_core" || $packagedir == "artdaq_utilities" ]]; then
    dependent_packages=""
else
    echo "This script can't make a dev-version for package \"$packagedir\" because it's not good enough. Exiting..." >&2
    exit 1
fi

sed -i -r 's/^(\s*parent\s+'$package'\s+)[^ #]+(.*)/\1'$tagname'\2/' srcs/$packagedir/ups/product_deps

for dependent_package in $dependent_packages;  do

    echo "Checking $dependent_package"
    dependent_package_dashes=$( echo $dependent_package | sed -r 's/_/-/g' )
    
    commit_or_version=$( sed -r -n 's/^\s*'$dependent_package_dashes' commit\/version: (\S+).*/\1/p' $run_records_dir/$run_tested/metadata.txt   )

    if [[ -z $commit_or_version ]]; then
	echo "Unable to determine the commit/dev-version/version of dependent package $dependent_package used in run $run_tested; exiting..." >&2
	exit 1
    fi

    if [[ ${#commit_or_version} == 40 ]]; then  # it's a commit hash

	if [[ -d srcs/$dependent_package ]]; then
	    cd srcs/$dependent_package

	    dependent_package_tagname=$( git tag --points-at HEAD )
	    if [[ -z $dependent_package_tagname ]]; then
		dependent_package_tagname=$( git tag --points-at $commit_or_version )

		if [[ -z $dependent_package_tagname ]]; then
		    echo "Unable to determine the tag of dependent package $dependent_package which repesents the code used in run $run_tested; exiting..."
		    exit 1
		fi
	    fi

	    if [[ -n $( echo $dependent_package_tagname | grep -E " " ) ]]; then
		echo "Error: more than one tag - $dependent_package_tagname - points to the code for $dependent_package which was used in $run_tested. Exiting..." >&2
		exit 1
	    fi

	    if ! [[ $dependent_package_tagname =~ ^v20[0-9]{2}_[0-9]{2}_[0-9]{2}.*$ || $dependent_package_tagname =~ ^v[0-9_]$ ]]; then
		echo "The tag this script determined corresponded to the code for $dependent_package in run $run_tested does not appear to be an expected format (either v<yyyy>_<mm>_<dd>_<branchname>_<descriptive tag>, v<yyyy>_<mm>_<dd>_<branchname>, or a version). This may be because no such tag exists; check $development_area_from_run/srcs/$packagedir" >&2 
		exit 1
	    fi

	    files_changed_between_commit_and_tag=$( git diff $dependent_package_tagname $commit_or_version --name-status | grep -v ups/product_deps | wc -l)  # What we really care about is that the tag describes the functioning code used during the run, so it's OK if product_deps is modified

	    if [[ $files_changed_between_commit_and_tag > 0 ]]; then
		
		git diff $dependent_package_tagname $commit_or_version --name-status 

		cat<<EOF >&2
Although this script's best guess as to the tag corresponding to commit $commit_or_version for dependent package 
$dependent_package_tagname is "$dependent_package_tagname", it appears that at least one file that isn't product_deps 
has been modified between the tag and the commit (see right above). Exiting...

EOF
		exit 1

	    fi

	    if [[ -z $dependent_package_tagname ]]; then
		echo "Unable to find a tag corresponding to commit $commit_or_version of dependent package $dependent_package, used in run $run_tested. Exiting..." >&2
		exit 1
	    fi
	    cd ../..
	else
	    echo "Unable to find srcs/$dependent_package_dir in $PWD. Exiting..." >&2
	    exit 1
	fi

    else
	dependent_package_tagname=$commit_or_version
    fi

    echo "Tag for dependent package $dependent_package found to be $dependent_package_tagname for code used in run $run_tested" 
    if [[ "$packagedir" != "dune_raw_data" ]]; then
	sed -i -r 's/^(\s*'$dependent_package'\s+)\S+(.*)/\1'$dependent_package_tagname'\2/' srcs/$packagedir/ups/product_deps
    else
	sed -i -r 's/^(\s*'$dependent_package'\s+)\S+(.*online\s*$)/\1'$dependent_package_tagname'\2/' srcs/$packagedir/ups/product_deps
    fi
done

echo "Finished checking dependent packages"

cd srcs/$packagedir
git diff HEAD
git add ups/product_deps
git commit -m "New dev-version $tagname, based on code tested in run ${run_tested}. This automatic update of product_deps has been done by "$(basename $0)" executed under user account \"$USER\""

if [[ -z $( git tag | grep -E "^$tagname\s*$" ) ]]; then

    if [[ -n $( git diff HEAD ) ]]; then
	echo "It appears that there's uncommitted code in $PWD; exiting..." >&2
	exit 1
    fi

    if [[ -n $( git diff HEAD -- ups/product_deps ) ]]; then

	git add ups/product_deps
	git commit -m "JCF: this commit was automatically performed by the script "$(basename $0)", bumping the artdaq dev-version to $tagname"

	if [[ "$?" != "0" ]]; then
	    echo "There was a problem attempting to commit the automatic update of ups/product_deps in $PWD; please take a look. Exiting..." >&2
	    exit 1
	fi

	echo "Automatically modified ups/product_deps in $PWD to reflect the new dev-version $tagname and performed a local commit"

    fi

    git tag $tagname

    if [[ "$?" != "0" ]]; then
	echo "A failure appears to have occurred trying to tag the automatically committed update of ups/product_deps in $PWD; exiting..." >&2
	exit 1
    fi

else 

    if [[ -n $( git diff $tagname ) ]]; then
	echo "Error: there's already a tag \"$tagname\", and it refers to code which is different than what's in $PWD. Exiting..." >&2
	exit 1
    fi
fi

cd ..  # to the ./srcs directory

# Modify the CMakeLists.txt in the srcs directory so we only build the target package and its dependencies

cp -p CMakeLists.txt CMakeLists.txt.backup
sed -i -r 's/^(\s*ADD_SUBDIRECTORY.*)/#\1/' CMakeLists.txt

for pkg in $packagedir $dependent_packages ; do
    pkg=$( echo $pkg | sed -r 's/ //g' ) # strip whitespace
    sed -i -r 's/^#(\s*ADD_SUBDIRECTORY\s*\('$pkg'\).*)/\1/' CMakeLists.txt
done

cd ..

if [[ -e $sourceme_for_build ]]; then
    . $sourceme_for_build
else
    echo "JCF: I made an assumption in this script which doesn't hold; can't find $sourceme_for_build from directory $PWD. Exiting..." >&2
    exit 1
fi

mrb z 
mrbsetenv
export MRB_INSTALL=/nfs/sw/artdaq/products_dev

if [[ ! -d /nfs/sw/artdaq/products_dev/$packagedir ]]; then
    mkdir /nfs/sw/artdaq/products_dev/$packagedir
    chmod go+w /nfs/sw/artdaq/products_dev/$packagedir # So others can install dev-versions as well
fi

mrb i -j16

cp -p srcs/CMakeLists.txt.backup srcs/CMakeLists.txt
cat<<EOF

If the build was successful, try running:

. /nfs/sw/artdaq/products/setup
. /nfs/sw/artdaq/products_dev/setup
ups list -aK+ $packagedir $tagname

If that looks OK, cd into ./srcs/$packagedir, check that the
automatically-updated product_deps looks fine, and then push the code
and the associated $tagname tag to the central repo at Fermilab.

EOF

exit 0

