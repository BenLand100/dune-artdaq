#!/bin/bash

# Get a ticket!
KEYTAB=/var/adm/krb5/lbnedaq.keytab
KEYUSE=`/usr/krb5/bin/klist -k ${KEYTAB} | grep FNAL.GOV | head -1 | cut -c 5- | cut -f 1 -d /`
/usr/krb5/bin/kinit -5 -A  -kt ${KEYTAB} ${KEYUSE}/dune/`hostname`@FNAL.GOV

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
	cp -r $directory /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring

	# Add this new directory to index.html so it's displayed on web
	RECENTRUN=`sed '7q;d' /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html`
	if [[ ! ${RECENTRUN} == "</br><a href=\"Run${run}Subrun${subrun}"* ]]; then
	    sed -i "7i</br><a href=\"Run${run}Subrun${subrun}\">Run ${run}, Subrun ${subrun} (updated ${date})</a>" /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html
	fi

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

    cp evd.png /web/sites/lbne-dqm.fnal.gov/htdocs/EventDisplay/
    rm -f event

fi
