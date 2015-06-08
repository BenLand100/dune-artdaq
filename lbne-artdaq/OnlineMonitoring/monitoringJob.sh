# Script to copy files across to the lbnegpvm node (where the
# lbne-daq.fnal.gov server is mounted) so they can be displayed
# on the web.

run=${1}
subrun=${2}
monitoringDir=${3}
outputDir=Run${run}Subrun${subrun}
host=wallbank@lbnegpvm01.fnal.gov
hostDir=/web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring

cd $monitoringDir
touch ${outputDir}
rm -rf ${outputDir}
mkdir ${outputDir}

cp index.html $outputDir
cp *png $outputDir
scp -r $outputDir $host:$hostDir
echo "</br><a href=\"${outputDir}\">Run ${run}, Subrun ${subrun}</a>" | ssh $host 'cat >> /web/sites/lbne-dqm.fnal.gov/htdocs/OnlineMonitoring/index.html' #${hostDir}/index.html'
rm -rf ${outputDir}