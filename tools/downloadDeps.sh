#!/bin/bash
# 
# downloadArtDaq.sh <product directory>


# JCF, 8/1/14

# For now, we'll just hardwire in the needed packages; something more
# sophisticated may be used in the future as package dependencies change

# 8/19/14 Packages downloaded in this version of the file are for
# lbne-artdaq v0_03_00


productdir=${1}

starttime=`date`

cd ${productdir}

# Some packages (lbne_raw_data) aren't yet on oink; we'll need to
# manually download them elsewhere 

#artdaq v1_11_00 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:eth:prof

prods="\
artdaq_core v1_02_01 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
art v1_10_00b -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
boost v1_55_0 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
cetlib v1_06_02 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
cetpkgsupport v1_05_02 -f NULL -z ${productdir} -g current
clhep v2_1_4_1 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
cmake v2_8_12_2 -f Linux64bit+2.6-2.12 -z ${productdir}
cpp0x v1_04_05 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
cppunit v1_12_1 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
fftw v3_3_3 -f Linux64bit+2.6-2.12 -z ${productdir} -q prof
fhiclcpp v2_19_05 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
gcc v4_8_2 -f Linux64bit+2.6-2.12 -z ${productdir}
gccxml v0_9_20131217 -f Linux64bit+2.6-2.12 -z ${productdir}
libxml2 v2_9_1 -f Linux64bit+2.6-2.12 -z ${productdir} -q prof
messagefacility v1_11_10 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
mpich v3_1 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
python v2_7_6 -f Linux64bit+2.6-2.12 -z ${productdir}
root v5_34_18d -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
sqlite v3_08_03_00 -f Linux64bit+2.6-2.12 -z ${productdir}
tbb v4_2_3 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
xmlrpc_c v1_25_28 -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof
xrootd v3_3_4a -f Linux64bit+2.6-2.12 -z ${productdir} -q e5:prof "

# Some tarfiles have names that deviate from the standard "template",
# so we can't use the download function's algorithm

prods2="\
cetbuildtools/cetbuildtools-3.13.00-noarch.tar.bz2
smc_compiler/smc_compiler-6.1.0-noarch.tar.bz2
TRACE/TRACE-3.03.03-slf6.tar.bz2
ups/ups-upd-5.0.5-slf6-x86_64.tar.bz2"

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
         vv=`echo $vv | sed -e 's/^v//;s/_/./g'`
         qq=`echo $qq | sed -e 's/:/-/g'`
         url=http://oink.fnal.gov/distro/packages/$pp/$pp-$vv-${ff}${qq:+-$qq}.tar.bz2
         echo url=$url
         wget -O- $url 2>/dev/null | tar xjf -
     done
}

cd ${productdir}

for packagestring in `echo $prods2 | tr " " "\n"`; do
    url=http://oink.fnal.gov/distro/packages/$packagestring
    echo url=$url
    wget -O- $url 2>/dev/null | tar xjf -
done

download ${productdir} "$prods"

echo -ne "\a"

endtime=`date`

echo $starttime
echo $endtime

exit 0

