#!/bin/bash

#KEYTAB=/var/adm/krb5/`/usr/krb5/bin/kcron -f`
KEYTAB=/var/adm/krb5/lbnedaq_cron.keytab
KEYUSE=`/usr/krb5/bin/klist -k ${KEYTAB} | grep FNAL.GOV | head -1 | cut -c 5- | cut -f 1 -d /`
/usr/krb5/bin/kinit -5 -A  -kt ${KEYTAB} ${KEYUSE}/dune/`hostname`@FNAL.GOV

phost=lbnedaq@lbnegpvm01.fnal.gov

# Look at the monitoring plots
cd /data/lbnedaq/monitoring

for directory in `find . -type d`
do
    if [ -e ${directory}/run ]; then

	# Get run/subrun
	read -r runsubrun < ${directory}/run
	rm -f $directory/run
	rsr=( ${runsubrun} )
	run=${rsr[0]}
	subrun=${rsr[1]}

	date=`date +"%A %B %_d %Y, %H:%M (%Z)"`

	# Copy the directory to the server
	scp -r $directory ${phost}:/web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring

	# Add this new directory to index.html so it's displayed on web
	ssh ${phost} "sed -i '5i</br><a href=\"Run${run}Subrun${subrun}\">Run ${run}, Subrun ${subrun} (uploaded ${date})</a>' /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html"

	# Tidy up
	# 18Jun -- copy files into the monitoring directory to keep only the most recent run
	# cp $directory/*png .
	# cp $directory/*root .
	# rm $directory -rf

    fi
done

# Look at event display
cd /data/lbnedaq/eventDisplay

if [ -e event ]; then

    scp ${phost}:/web/sites/lbne-dqm.fnal.gov/htdocs/EventDisplay
    rm -f event

fi

/usr/krb5/bin/kdestroy