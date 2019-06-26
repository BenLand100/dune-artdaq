//Protection necessary to declare any structs
#ifndef ADJACENCYALGORITHMS_H
#define ADJACENCYALGORITHMS_H

//some standard C++ includes

//See AdjacencyAlgorithms.cpp for full function definitions as well as credit for the code.
#include <iostream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;

//Mimicking PTMP messaging system, makes sorting more efficient as well... Shoulda probably stuck with this sort of thing from the start, but oh well... might be easiest to match his into this struct so that there is no inherent dependencies in these functions on PTMP... WOULD BE BEST IF PIERRE WERE AROUND...
struct TP{
  uint32_t channel;
  uint64_t tstart;
  uint32_t tspan;
  uint32_t adcsum;
  uint32_t adcpeak;
  uint32_t flags;
};

//Functions for non-struct handling, adjacency, etc.
vector<vector<unsigned int>> ResortHitsByAPA(vector<unsigned int> channel_vec, vector<unsigned int> sorting_vec);
vector<vector<unsigned int>> ResortHitsByWindow(vector<unsigned int> time_vec, vector<unsigned int> sorting_vec);
vector<vector<unsigned int>> ResortHitsByTPC(vector<unsigned int> channel_vec, vector<unsigned int> sorting_vec, unsigned int APA);
vector<vector<unsigned int>> SortByTick(vector<unsigned int> time_vec, vector<unsigned int> sorting_vec, unsigned int window);

vector<unsigned int> FindMaxTOTCluster(vector<vector<unsigned int>> tots_per_window);
vector<unsigned int> FindMaxSumADCCluster(vector<vector<unsigned int>> sum_adc_per_window);
vector<unsigned int> FindMaxTOTPrim(vector<unsigned int> tots);
vector<unsigned int> FindMaxSumADCPrim(vector<unsigned int> sum_adc_tpc_1, vector<unsigned int> sum_adc_tpc_2);

int WindowCounting(vector<unsigned int> time_vec);
int RightAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window);
int LeftAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window);
int SameAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingCluster(vector<vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingPrim(vector<unsigned int> channels);
vector<int> AdjacentSameWindowCountingPrimWithEverything(vector<unsigned int> channels, vector<unsigned int> adcs, vector<unsigned int> tots);
int TimeAdjacentCounting(vector<vector<unsigned int>> channels, vector<vector<unsigned int>> adcs);
vector<int> TimeAdjacentCountingWithEverything(vector<vector<unsigned int>> channels, vector<vector<unsigned int>> adcs, vector<vector<unsigned int>> tots);

vector<int> PDUNEAdjacencyWithEverything(vector<unsigned int> channels,vector<unsigned int> times, vector<unsigned int> adcs, vector<unsigned int> tots);

//Function for conversion from present format to structs, will allow us to never have to change the skim etc. which would involve more convoluted library matching etc., so this is dandy...
bool CompareTPs (TP TP1, TP TP2);
bool CompareTPSets (vector<TP> TPs1, vector<TP> TPs2);

vector<TP> HitsToTPs(vector<unsigned int> channels, vector<unsigned int> times, vector<unsigned int> tots, vector<unsigned int> adcs);
vector<TP> CleanTPs(vector<TP> TPs);

//Functions for struct handling, adjacency, etc.
vector<vector<TP>> ResortTPsByAPA(vector<TP> TPs);
vector<vector<TP>> ResortTPsByWindow(vector<TP> TPs, int channel_sort=0);

/* Not important presently, more important than below, just not used much really... WILL NEED TO RENAME
vector<vector<TP>> ResortHitsByTPC(vector<TP> TPs, unsigned int APA);
vector<vector<TP>> SortByTick(vector<TP> TPs, unsigned int window);
*/

/* Don't plan on needing again, may unpack for testing at some point, don't have time to reqrite everything right now... WILL NEED TO RENAME

vector<unsigned int> FindMaxTOTCluster(vector<vector<TP>> TPs);
vector<unsigned int> FindMaxSumADCCluster(vector<vector<unsigned int>> sum_adc_per_window);
vector<unsigned int> FindMaxTOTPrim(vector<unsigned int> tots);
vector<unsigned int> FindMaxSumADCPrim(vector<unsigned int> sum_adc_tpc_1, vector<unsigned int> sum_adc_tpc_2);

int WindowCounting(vector<unsigned int> time_vec);
int RightAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window);
int LeftAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window);
int SameAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingCluster(vector<vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingPrim(vector<unsigned int> channels);

int TimeAdjacentCounting(vector<vector<unsigned int>> channels, vector<vector<unsigned int>> adcs);
*/

vector<int> PDUNEAdjacency(vector<TP> TPs);

/*Not presently necessary, more important than above set... WILL NEED TO RENAME
vector<int> PDUNEClustering(vector<TP> TPs);

vector<int> AdjacentSameWindowCountingPrimWithEverything(vector<unsigned int> channels, vector<unsigned int> adcs, vector<unsigned int> tots);

vector<int> TimeAdjacentCountingWithEverything(vector<vector<unsigned int>> channels, vector<vector<unsigned int>> adcs, vector<vector<unsigned int>> tots);
*/

#endif /* ADJACENCYALGORITHMS_H */
