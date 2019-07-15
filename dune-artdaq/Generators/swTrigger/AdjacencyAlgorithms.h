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
std::vector<std::vector<unsigned int>> ResortHitsByAPA(std::vector<unsigned int> channel_vec, std::vector<unsigned int> sorting_vec);
std::vector<std::vector<unsigned int>> ResortHitsByWindow(std::vector<unsigned int> time_vec, std::vector<unsigned int> sorting_vec);
std::vector<std::vector<unsigned int>> ResortHitsByTPC(std::vector<unsigned int> channel_vec, std::vector<unsigned int> sorting_vec, unsigned int APA);
std::vector<std::vector<unsigned int>> SortByTick(std::vector<unsigned int> time_vec, std::vector<unsigned int> sorting_vec, unsigned int window);

std::vector<unsigned int> FindMaxTOTCluster(std::vector<std::vector<unsigned int>> tots_per_window);
std::vector<unsigned int> FindMaxSumADCCluster(std::vector<std::vector<unsigned int>> sum_adc_per_window);
std::vector<unsigned int> FindMaxTOTPrim(std::vector<unsigned int> tots);
std::vector<unsigned int> FindMaxSumADCPrim(std::vector<unsigned int> sum_adc_tpc_1, std::vector<unsigned int> sum_adc_tpc_2);

int WindowCounting(std::vector<unsigned int> time_vec);
int RightAdjacentLaterWindowCounting(std::vector<std::vector<unsigned int>> channels_per_window);
int LeftAdjacentLaterWindowCounting(std::vector<std::vector<unsigned int>> channels_per_window);
int SameAdjacentLaterWindowCounting(std::vector<std::vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingCluster(std::vector<std::vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingPrim(std::vector<unsigned int> channels);
std::vector<int> AdjacentSameWindowCountingPrimWithEverything(std::vector<unsigned int> channels, std::vector<unsigned int> adcs, std::vector<unsigned int> tots);
int TimeAdjacentCounting(std::vector<std::vector<unsigned int>> channels, std::vector<std::vector<unsigned int>> adcs);
std::vector<int> TimeAdjacentCountingWithEverything(std::vector<std::vector<unsigned int>> channels, std::vector<std::vector<unsigned int>> adcs, std::vector<std::vector<unsigned int>> tots);

std::vector<int> PDUNEAdjacencyWithEverything(std::vector<unsigned int> channels,std::vector<unsigned int> times, std::vector<unsigned int> adcs, std::vector<unsigned int> tots);

//Function for conversion from present format to structs, will allow us to never have to change the skim etc. which would involve more convoluted library matching etc., so this is dandy...
bool CompareTPs (TP TP1, TP TP2);
bool CompareTPSets (std::vector<TP> TPs1, std::vector<TP> TPs2);

std::vector<TP> HitsToTPs(std::vector<unsigned int> channels, std::vector<unsigned int> times, std::vector<unsigned int> tots, std::vector<unsigned int> adcs);
std::vector<TP> CleanTPs(std::vector<TP> TPs);

//Functions for struct handling, adjacency, etc.
std::vector<std::vector<TP>> ResortTPsByAPA(std::vector<TP> TPs);
std::vector<std::vector<TP>> ResortTPsByWindow(std::vector<TP> TPs, int channel_sort=0);

/* Not important presently, more important than below, just not used much really... WILL NEED TO RENAME
std::vector<std::vector<TP>> ResortHitsByTPC(std::vector<TP> TPs, unsigned int APA);
std::vector<std::vector<TP>> SortByTick(std::vector<TP> TPs, unsigned int window);
*/

/* Don't plan on needing again, may unpack for testing at some point, don't have time to reqrite everything right now... WILL NEED TO RENAME

std::vector<unsigned int> FindMaxTOTCluster(std::vector<std::vector<TP>> TPs);
std::vector<unsigned int> FindMaxSumADCCluster(std::vector<std::vector<unsigned int>> sum_adc_per_window);
std::vector<unsigned int> FindMaxTOTPrim(std::vector<unsigned int> tots);
std::vector<unsigned int> FindMaxSumADCPrim(std::vector<unsigned int> sum_adc_tpc_1, std::vector<unsigned int> sum_adc_tpc_2);

int WindowCounting(std::vector<unsigned int> time_vec);
int RightAdjacentLaterWindowCounting(std::vector<std::vector<unsigned int>> channels_per_window);
int LeftAdjacentLaterWindowCounting(std::vector<std::vector<unsigned int>> channels_per_window);
int SameAdjacentLaterWindowCounting(std::vector<std::vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingCluster(std::vector<std::vector<unsigned int>> channels_per_window);
int AdjacentSameWindowCountingPrim(std::vector<unsigned int> channels);

int TimeAdjacentCounting(std::vector<std::vector<unsigned int>> channels, std::vector<std::vector<unsigned int>> adcs);
*/

std::vector<int> PDUNEAdjacency(std::vector<TP> TPs);

/*Not presently necessary, more important than above set... WILL NEED TO RENAME
std::vector<int> PDUNEClustering(std::vector<TP> TPs);

std::vector<int> AdjacentSameWindowCountingPrimWithEverything(std::vector<unsigned int> channels, std::vector<unsigned int> adcs, std::vector<unsigned int> tots);

std::vector<int> TimeAdjacentCountingWithEverything(std::vector<std::vector<unsigned int>> channels, std::vector<std::vector<unsigned int>> adcs, std::vector<std::vector<unsigned int>> tots);
*/

#endif /* ADJACENCYALGORITHMS_H */
