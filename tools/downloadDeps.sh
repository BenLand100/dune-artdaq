#!/bin/bash
# 
# downloadArtDaq.sh <product directory>


# JCF, 8/1/14

# For now, we'll just hardwire in the needed packages; something more
# sophisticated may be used in the future as package dependencies change

# 9/29/14 Packages downloaded in this version of the file are for
# lbne-artdaq v0_05_00 (e6:s5 compilation)


productdir=${1}
basequal=${2}
build_type=${3}  # "prof" or "debug"

basequal2=`echo ${basequal} | sed -r '/.*:.*:.*/s!:s5:eth!!' | sed -r 's!:eth!!'`
basequal3=`echo ${basequal} | sed -e s!:eth!!g`

starttime=`date`

cd ${productdir}

# What about mvapich2?

prods="\
art v1_12_01 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
boost v1_56_0 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
cetlib v1_07_03 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
cetpkgsupport v1_07_01 -f NULL -z ${productdir} -g current
clhep v2_2_0_3 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
cmake v3_0_1 -f Linux64bit+2.6-2.12 -z ${productdir}
cpp0x v1_04_08 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
cppunit v1_12_1a -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
fftw v3_3_4 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${build_type}
fhiclcpp v3_01_02 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
gcc v4_9_1 -f Linux64bit+2.6-2.12 -z ${productdir}
gccxml v0_9_20140718 -f Linux64bit+2.6-2.12 -z ${productdir}
libxml2 v2_9_1a -f Linux64bit+2.6-2.12 -z ${productdir} -q ${build_type}
messagefacility v1_11_15 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
mpich v3_1_2 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
python v2_7_8 -f Linux64bit+2.6-2.12 -z ${productdir}
root v5_34_21a -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
sqlite v3_08_05_00 -f Linux64bit+2.6-2.12 -z ${productdir}
tbb v4_2_5 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
xmlrpc_c v1_25_30 -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type}
xrootd v3_3_4b -f Linux64bit+2.6-2.12 -z ${productdir} -q ${basequal2}:${build_type} "

# Some tarfiles have names that deviate from the standard "template",
# so we can't use the download function's algorithm

prods2="\
cetbuildtools/v4_02_02/cetbuildtools-4.02.02-noarch.tar.bz2
smc_compiler/v6_1_0/smc_compiler-6.1.0-noarch.tar.bz2
TRACE/v3_03_03/TRACE-3.03.03-slf6.tar.bz2
ups/v5_0_5/ups-upd-5.0.5-slf6-x86_64.tar.bz2"

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
         wget -O- $url 2>/dev/null | tar xjf -
     done
}

cd ${productdir}

for packagestring in `echo $prods2 | tr " " "\n"`; do
    url=http://scisoft.fnal.gov/scisoft/packages/$packagestring
    echo url=$url
    wget -O- $url 2>/dev/null | tar xjf -
done

download ${productdir} "$prods"

echo -ne "\a"

endtime=`date`

echo $starttime
echo $endtime

exit 0

