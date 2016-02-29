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

	# Remove the root file from the web area (not enough space on the web to keep them; they are saved in the /data/lbnedaq/monitoring directory)
	rm -f /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/Run${run}Subrun${subrun}/*root

	# Add this new directory to index.html so it's displayed on web
	RECENTRUN=`sed '8q;d' /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html`
	if [[ ! ${RECENTRUN} == "</br><a href=\"Run${run}Subrun${subrun}"* ]]
	then
	    sed -i "8i</br><a href=\"Run${run}Subrun${subrun}\">Run ${run}, Subrun ${subrun} (updated ${date})</a>" /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html
	else
	    sed -ie '8d' /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html
	    sed -i "8i</br><a href=\"Run${run}Subrun${subrun}\">Run ${run}, Subrun ${subrun} (updated ${date})</a>" /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html
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

    # Get run/subrun
    read -r runsubrunevent < event
    rm -f event
    rsre=( ${runsubrunevent} )
    run=${rsre[0]}
    subrun=${rsre[1]}
    evt=${rsre[2]}

    date=`date +"%A %B %_d %Y, %H:%M (%Z)"`

    cp evd.png /web/sites/lbne-dqm.fnal.gov/htdocs/EventDisplay/
    cp evd.png archive/evd_run${run}_subrun${subrun}_event${evt}.png

    # Update the html
    sed -ie '15d' /web/sites/lbne-dqm.fnal.gov/htdocs/EventDisplay/index.html
    sed -i "15i<h2 align=\"center\">Run ${run}, Subrun ${subrun}</h2>" /web/sites/lbne-dqm.fnal.gov/htdocs/EventDisplay/index.html
    sed -ie '16d' /web/sites/lbne-dqm.fnal.gov/htdocs/EventDisplay/index.html
    sed -i "16i<center>Last updated ${date} for event ${evt}</center>" /web/sites/lbne-dqm.fnal.gov/htdocs/EventDisplay/index.html

fi
