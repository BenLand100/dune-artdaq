#!/bin/bash
# 
# downloadArtDaq.sh <product directory>


# JCF, 8/1/14

# For now, we'll just hardwire in the needed packages; something more
# sophisticated may be used in the future as package dependencies change


productdir=${1}
basequal=${2}
build_type=${3}  # "prof" or "debug"

basequal2=`echo ${basequal} | sed -e s/:eth//`

starttime=`date`

cd ${productdir}

prods="\
mpich v3_1_2a -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
xmlrpc_c v1_25_30a -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}"

# Some tarfiles have names that deviate from the standard "template",
# so we can't use the download function's algorithm

prods2="\
smc_compiler/v6_1_0/smc_compiler-6.1.0-noarch.tar.bz2
TRACE/v3_05_00/TRACE-3.05.00-slf6.tar.bz2
cetbuildtools/v4_09_02/cetbuildtools-4.09.02-noarch.tar.bz2
cmake/v3_2_1/cmake-3.2.1-slf6-x86_64.tar.bz2
cetpkgsupport/v1_08_05/cetpkgsupport-1.08.05-noarch.tar.bz2
"

# $1=prod_area $2="prod_lines"

download() 
{   prod_area=$1
     prod_lines=$2
     cd $1

     test -f setup || echo invalid products area
     test -f setup || return
     echo "$2" | while read pp vv dashf ff dashz zz dashq qq rest;do
         test "x$dashq" = x-q || qq=
         #UPS_OVERRIDE= ups exist -j $pp $vv -f $ff ${qq:+-q$qq} && echo $pp $vv exists && continue
         case $ff in
         Linux64bit+2.6-2.12) ff=slf6-x86_64;;
         NULL)                ff=noarch;;
         *)   echo ERROR: unknown flavor $ff; return;;
         esac
         vvdot=`echo $vv | sed -e 's/^v//;s/_/./g'`
         qq=`echo $qq | sed -e 's/:/-/g'`
         url=http://scisoft.fnal.gov/scisoft/packages/$pp/$vv/$pp-$vvdot-${ff}${qq:+-$qq}.tar.bz2
         echo url=$url
         wget -O- -o/dev/null $url | tar xjf -
     done
}

cd ${productdir}

# 16-Oct-2014, KAB: switched to using pullProducts for basic art suite
export savedPRODUCTS=${PRODUCTS}
installDir=`pwd`
simpleQual=`echo ${basequal} | sed 's/:eth//g' | sed 's/eth://g' | sed 's/ib://g' | sed 's/:ib//g'`
artVersion=v1_12_05
pullScript=pullProducts
url=http://scisoft.fnal.gov/scisoft/bundles/tools/pullProducts
echo url=$url
wget $url 2>/dev/null
chmod +x ${pullScript}
mkdir tarfiles
cd tarfiles
export PRODUCTS=${installDir}
echo "../${pullScript} ${installDir} slf6 art-${artVersion} ${simpleQual} ${build_type}"
../${pullScript} ${installDir} slf6 art-${artVersion} ${simpleQual} ${build_type}
export PRODUCTS=${savedPRODUCTS}
cd ${installDir}
rm -rf tarfiles

for packagestring in `echo $prods2 | tr " " "\n"`; do
    url=http://scisoft.fnal.gov/scisoft/packages/$packagestring
    echo url=$url
    wget -O- -o/dev/null $url | tar xjf -
done

download ${productdir} "$prods"

echo -ne "\a"

endtime=`date`

echo $starttime
echo $endtime

exit 0

