#!/bin/bash

howmany=$1

basedir="/scratch/phrodrig"
#fnaldir=/dune/data/users/rodriges/felix-long-readout
eosdir=/eos/user/p/phrodrig/dune/protodune/felix-long-readout

pushd ${basedir}

date

for i in `seq ${howmany}`; do

    dump_link ${basedir} 0 24000000 32 || exit 1

    #pushd ${basedir} || exit 1
    rawfile=$(ls raw-channel-*.dat)
    echo "raw file is ${rawfile}"
    if [ -z "${rawfile}" -o ! -f "${rawfile}" ]; then
        break
    fi

    date
    echo "Gzipping..."
    pigz $rawfile
    zipfile=${rawfile}.gz
    if [ ! -e "${zipfile}" ]; then
        break
    fi
    date

    #echo "Copying to eos..."
    #scp ${zipfile} lxplus:${eosdir}/

    # \rm ${rawfile}
    #\rm ${zipfile}
done

popd
