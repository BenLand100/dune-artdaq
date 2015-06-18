#!bin/bash

host=wallbank@lbnegpvm01.fnal.gov

cd /data/lbnedaq/monitoring

for directory in `find . -type d`
do
    if [ -e ${directory}/run ]; then

	# Get run/subrun
	read -r runsubrun < ${directory}/run
	rm -f $directory/run
	run=${runsubrun[0]}
	subrun=${runsubrun[1]}

	# Copy the directory to the server
	scp -r $directory ${host}:/web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring

	# Add this new directory to index.html so it's displayed on web
	echo "</br><a href=\"Run${run}Subrun${subrun}\">Run ${run}, Subrun ${subrun}</a>" | ssh $host 'cat >> /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html'

	# Tidy up
	# 18Jun -- copy files into the monitoring directory to keep only the most recent run
	cp $directory/*png .
	cp $directory/*root .
	rm $directory -rf

    fi

done