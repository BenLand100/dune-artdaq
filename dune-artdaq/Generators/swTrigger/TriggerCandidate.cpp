#include "AdjacencyAlgorithms.h"
#include "TriggerCandidate.h"


//convenient for us! let's not bother with std namespaces!
using namespace std;

/*
The following function takes some set of hits for a single APA, sorts them as necessary, and then performs the Adjacency/Clustering with the full calculation as specified in AdjacencyAlgorithms.cpp for the protoDUNE-specific scheme for horizontal muons.
The output mirroring a trigger candidate (sans metadata) will have the 8 outputs corresponding to:
0-3) TADC, Adjacency/Clustering, WADC, TOT 
4-7) first channel APA-wise of largest cluster, its associated tick, last channel APA-wise of largest cluster, its associated tick
*/

//int adj_cut = -1;
int adj_thresh = 50;
//unsigned int tot_cut = -1;
int tot_thresh = 0;
int adc_cut = 0;
int adc_thresh = 0;
int wire_cut = 0;
int wire_thresh = 0;

vector<int> TriggerCandidate(vector<TP> TPs, int clustering){
  vector<int> fail; //returned if the trigger candidate is not issued. The output from PDUNEAdjacencyWithEverything is returned if the Trigger is issued.
  vector<int> candidate;

  //PERFORM ANY SORTING NECESSARY, RIGHT NOW, NONE ASSUMED TO BE NECESSARY

  //Perform Adjacency/Clustering
  if (clustering == 1) {
    cout << "Clustering is not yet implemented in the code for protoDUNE and no candidate will be issued." << endl;
    return fail;
    //candidate = PDUNEClusteringWithEverything(channels, times, adcs, tots);
  }
  else candidate = PDUNEAdjacency(TPs);

  if (candidate.at(0) > adj_thresh){
    //    candidate.push_back(1);
    return candidate;
  }
  else{
    //    candidate.push_back(0);
    return fail;
  }
}

vector<int> TriggerCandidateHits(vector<unsigned int> channels, vector<unsigned int> times, vector<unsigned int> adcs, vector<unsigned int> tots, int clustering){
  vector<int> fail; //returned if the trigger candidate is not issued. The output from PDUNEAdjacencyWithEverything is returned if the Trigger is issued.
  vector<int> candidate;

  //PERFORM ANY SORTING NECESSARY, RIGHT NOW, NONE ASSUMED TO BE NECESSARY

  //Perform Adjacency/Clustering
  if (clustering == 1) {
    cout << "Clustering is not yet implemented in the code for protoDUNE and no candidate will be issued." << endl;
    return fail;
    //candidate = PDUNEClusteringWithEverything(channels, times, adcs, tots);
  }
  else candidate = PDUNEAdjacencyWithEverything(channels, times, adcs, tots);

  if (candidate.at(0) > adj_thresh){
    //    candidate.push_back(1);
    return candidate;
  }
  else{
    //    candidate.push_back(0);
    return fail;
  }
}
