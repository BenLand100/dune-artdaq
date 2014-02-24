#!/bin/bash
# 
# downloadArtDaq.sh <product directory> <e2|e2:eth|nu:e2:eth|e4:eth> <debug|opt|prof>

thisdir=`pwd`
productdir=${1}
basequal=${2}
extraqual=${3}

# require qualifier
if [ -z ${basequal} ]
then
   echo "ERROR: qualifier not specified"
   echo "USAGE:  `basename ${0}` <product directory> <e2|e2:eth|nu:e2:eth|e4:eth> <debug|opt|prof>"
   exit 1
fi

if [ "${extraqual}" = "opt" ]
then
   qualdir=${qual}.${extraqual}
elif [ "${extraqual}" = "debug" ]
then
   qualdir=${qual}.${extraqual}
elif [ "${extraqual}" = "prof" ]
then
   qualdir=${qual}.${extraqual}
else
   echo "ERROR: please specify debug, opt, or prof"
   echo "USAGE:  `basename ${0}` <product directory> <e2|e2:eth|nu:e2:eth|e4:eth> <debug|opt|prof>"
   exit 1
fi

OS1=`uname`

if [ ${OS1} = "Darwin" ]
then
    plat=`uname -m`
    OSnum=`uname -r | cut -f1 -d"."`
    macos=`system_profiler -xml -detailLevel mini SPSoftwareDataType | grep Mac | cut -d ' ' -f 4 | cut -d . -f 1,2`
    macos1=`system_profiler -xml -detailLevel mini SPSoftwareDataType | grep Mac | cut -d ' ' -f 4 | cut -d . -f 1`
    macos2=`system_profiler -xml -detailLevel mini SPSoftwareDataType | grep Mac | cut -d ' ' -f 4 | cut -d . -f 2`
    #OS=${OS1}${OSnum}
    OS=mac${macos1}${macos2}
else
   if [ `lsb_release -d | cut -f2 | cut  -f1 -d" "` = "Ubuntu" ]
   then
      OS1=ub
      OSnum=`lsb_release -r | cut -f2 | cut -f1 -d"."`
      OS=${OS1}${OSnum}
      plat=`uname -m`
   elif [ ${OS1} = "Linux" ]
   then
      OSnum=`lsb_release -r | cut -f2 | cut -f1 -d"."`
      OS=${OS1}${OSnum}
      plat=`uname -p`
      # Scientific Linux - slf should work
      if [ `lsb_release -d | cut  -f3 -d" "` = "SL" ]
      then
	 OS=slf${OSnum}
      # Scientific Linux Fermi
      elif [ `lsb_release -d | cut  -f3 -d" "` = "SLF" ]
      then
	 OS=slf${OSnum}
      #
      elif [ `lsb_release -i | cut -f2` = "ScientificFermi" ]
      then
	 OS=slf${OSnum}
      # pretend that SL6 is the same as SLF6
      elif [ `lsb_release -i | cut -f2` = "Scientific" ]
      then
	 OS=slf${OSnum}
      # pretend that CentOS is SLF
      elif [ `lsb_release -i | cut -f2` = "CentOS" ]
      then
	 OS=slf${OSnum}
      # pretend that RedHatEnterpriseServer is SLF
      elif [ `lsb_release -i | cut -f2` = "RedHatEnterpriseServer" ]
      then
	 OS=slf${OSnum}
      # Scientific Linux CERN
      elif [ `lsb_release -d | cut  -f4 -d" "` = "SLC" ]
      then
	 OS=slc${OSnum}
      elif [ `lsb_release -d | cut  -f4 -d" "` = "LTS" ]
      then
	 OS=slf${OSnum}
      # unrecognized - pretend that this is SLF
      else
	 OS=slf${OSnum}
      fi
   fi
fi

subdir=${OS}.${plat}.${qualdir}
xqual=`echo ${basequal} | sed -e s%:%-%g`
xqual2=`echo ${xqual} | sed -e s%-eth%%g`

#echo "you MUST be logged in as products to execute this script"
echo "you are about to download the tarballs into $thisdir"
echo "tarballs will be unwound in ${productdir}"

set -x

  curl -O http://oink.fnal.gov/distro/relocatable-ups/ups-upd-5.0.1-${OS}-${plat}.tar.bz2
  curl -O http://oink.fnal.gov/distro/art/art_externals-1.00.04-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/art/art_suite-1.08.10-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/cetpkgsupport/cetpkgsupport-1.05.02-noarch.tar.bz2
#  wget  http://oink.fnal.gov/distro/packages/cetpkgsupport-1.05.02-noarch.tar.bz2 
#  echo $PWD $thisdir
#  ls -l cetpkgsupport-1.05.02-noarch.tar.bz2

  cd ${productdir}

  tar xf $thisdir/ups-upd-5.0.1-${OS}-${plat}.tar.bz2

  tar xf $thisdir/art_externals-1.00.04-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2

  tar xf $thisdir/art_suite-1.08.10-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2
  tar xf $thisdir/cetpkgsupport-1.05.02-noarch.tar.bz2


set +x

if [ "${basequal}" = "e2" ]
then
  set -x
  cd ${thisdir}
  curl -O http://oink.fnal.gov/distro/ds50/mvapich2_stub-1.7.0-noarch.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/ds50/ds50daq-0.01.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  cd ${productdir}
  tar xf $thisdir/mvapich2_stub-1.7.0-noarch.tar.bz2
  tar xf $thisdir/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  tar xf $thisdir/ds50daq-0.01.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  set +x
fi

if [ "${basequal}" = "e2:eth" ]
then
  set -x
  cd ${thisdir}
  curl -O http://oink.fnal.gov/distro/packages/mpich-3.0.2-${OS}-${plat}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/xmlrpc_c-1.25.18-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/gccxml-0.9.20130503-${OS}-${plat}-gcc47.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  cd ${productdir}
  tar xf $thisdir/mpich-3.0.2-${OS}-${plat}-${extraqual}.tar.bz2
  tar xf $thisdir/xmlrpc_c-1.25.18-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2
  tar xf $thisdir/gccxml-0.9.20130503-${OS}-${plat}-gcc47.tar.bz2
  tar xf $thisdir/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  set +x
fi

if [ "${basequal}" = "e4:eth" ]
then
  set -x
  cd ${thisdir}
  curl -O http://oink.fnal.gov/distro/art/daq_extras-1.00.04-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/gccxml/gccxml-0.9.20130621-${OS}-${plat}.tar.bz2
#  curl -O http://oink.fnal.gov/distro/packages/artdaq/artdaq-1.05.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2

  cd ${productdir}
  tar xf $thisdir/daq_extras-1.00.04-${OS}-${plat}-${xqual2}-${extraqual}.tar.bz2
  tar xf $thisdir/gccxml-0.9.20130621-${OS}-${plat}.tar.bz2
#  tar xf  $thisdir/artdaq-1.05.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  set +x
fi


if [ "${basequal}" = "mu2e:e2:eth" ]
then
  set -x
  cd ${thisdir}
  curl -O http://oink.fnal.gov/distro/art/mu2e_extras-0.06.05-noarch.tar.bz2
  curl -O http://oink.fnal.gov/distro/art/mu2e_extras-0.06.05-${OS}-${plat}-e2-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/splines-1.01.04-${OS}-${plat}-e2-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/mpich-3.0.2-${OS}-${plat}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  cd ${productdir}
  tar xf $thisdir/mu2e_extras-0.06.05-noarch.tar.bz2
  tar xf $thisdir/mu2e_extras-0.06.05-${OS}-${plat}-e2-${extraqual}.tar.bz2
  tar xf $thisdir/splines-1.01.04-${OS}-${plat}-e2-${extraqual}.tar.bz2
  tar xf $thisdir/mpich-3.0.2-${OS}-${plat}-${extraqual}.tar.bz2
  tar xf $thisdir/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  set +x
fi

if [ "${basequal}" = "nu:e2:eth" ]
then
  set -x
  cd ${thisdir}
  curl -O http://oink.fnal.gov/distro/art/nu_extras-0.06.05-noarch.tar.bz2
  curl -O http://oink.fnal.gov/distro/art/nu_extras-0.06.05-${OS}-${plat}-e2-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/mpich-3.0.2-${OS}-${plat}-${extraqual}.tar.bz2
  curl -O http://oink.fnal.gov/distro/packages/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  cd ${productdir}
  tar xf $thisdir/nu_extras-0.06.05-noarch.tar.bz2
  tar xf $thisdir/nu_extras-0.06.05-${OS}-${plat}-e2-${extraqual}.tar.bz2
  tar xf $thisdir/mpich-3.0.2-${OS}-${plat}-${extraqual}.tar.bz2
  tar xf $thisdir/artdaq-1.00.00-${OS}-${plat}-${xqual}-${extraqual}.tar.bz2
  set +x
fi

#cetbuildtools

for ver in v3_07_03
do
  dotver=`echo ${ver} | sed -e s%_%.%g | sed -e s%v%%`

  set -x
  cd $thisdir
  curl -O http://oink.fnal.gov/distro/packages/cetbuildtools/cetbuildtools-${dotver}-noarch.tar.bz2
  cd ${productdir}
  tar xf $thisdir/cetbuildtools-${dotver}-noarch.tar.bz2
  set +x

done

exit

