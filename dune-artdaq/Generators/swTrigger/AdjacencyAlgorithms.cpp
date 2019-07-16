#include "AdjacencyAlgorithms.h"

/***
The following code was written in its entirety by David Last and David Rivera of the University of Pennsylvania as of February 22, 2019.

Overall Comments:

There are two approaches which are being implemented for clustering in the below which will be referred to as Adjacency and Clustering in the relevant comments. Both only look at primitives from within some set-size time window (chosen to be 50 us as to combat Ar39). Adjacency then just asks how many adjacent hit wires are there in the window and for the window reports the largest number. For the Clustering, it is more complicated and will be described below. But it further breaks the window up into smaller chunks for clustering hits together, but still calculates total charge and maximum single primitive charge and time over threshold (TOT) for the whole window. This will be relevant prior to the detailed description of the algorithms.

Functions that are associated with the Adjacency method will end with "Prim" and that which is associated with Clustering will end with "Cluster". The exception to this rule is in the section of PRIMARY CLUSTERING FUNCTIONS. So they will be clearly explained. The functions for which this applies have mostly been replaced.

Presently due to speed and comparable selection ability, the method of "Adjacency" is favored.
***/

using namespace std;

/***Initialization of specific parameters for the protoDUNE geometry/digitization, event size in time.
 
nChannels and nColl should be the same for any APAs that follow the design for DUNE.
nticks is specific to everything being delivered in groupings by a drift and is only relevant to sorting functions.
***/

int nticks = 6000;
int windows = 100;
int nAPAs = 6;
int nChannels = 2560;
int nColl = 960;
int prim_ticks = 10;

unsigned int too_far = 2;

//Comparing two TPs
bool CompareTPs(TP TP1, TP TP2){
  if (TP1.channel == TP2.channel && TP1.tspan == TP2.tspan && TP1.tstart == TP2.tstart && TP1.adcsum == TP2.adcsum && TP1.adcpeak == TP2.adcpeak && TP1.flags == TP2.flags) return true;
  else return false;
}

bool CompareTPSets(vector<TP> TPs1, vector<TP> TPs2){
  if (TPs1.size() != TPs2.size()) return false;
  for (unsigned int i=0; i < TPs1.size(); ++i){
    if (!CompareTPs(TPs1.at(i),TPs2.at(i))) return false;
  }
  return true;
}

//Type Conversion from current LArSoft Hit trees from skims to new struct object, TP. No sorting done here.

vector<TP> HitsToTPs(vector<unsigned int> channels, vector<unsigned int> times, vector<unsigned int> tots, vector<unsigned int> adcs){
  vector<TP> TPs;
  for (unsigned int i=0; i < channels.size(); ++i){
    //Filling of zero is just so that none of these have any errors, the handling of any TP with an error will be handled elsewhere.
    TP TPtmp={channels.at(i),times.at(i),tots.at(i),adcs.at(i),adcs.at(i),0};
    TPs.push_back(TPtmp);
  }
  return TPs;
}

//Effectively removes any TPs which have error flags... Will investigate when running live... NEED TO TEST AT SOME POINT
vector<TP> CleanTPs(vector<TP> TPs){
  vector<TP> new_TPs;
  for (unsigned int i=0; i < TPs.size(); ++i){
    if (TPs.at(i).flags ==0){
      new_TPs.push_back(TPs.at(i));
    }
  }
  return new_TPs;
}

//Sorting Functions.

/***
ResortHitsByWindow comments:

Sorting a given vector of primitive characteristics (charge, time, channel) for any quantity of time into independent vectors of hits into the desired windows.
***/

vector<vector<unsigned int>> ResortHitsByWindow(vector<unsigned int> time_vec, vector<unsigned int> sorting_vec){
  vector<vector<unsigned int>> new_hits(nticks/windows+1);
  for (unsigned int i=0; i < time_vec.size(); ++i){
    new_hits.at((int)time_vec.at(i)/windows).push_back(sorting_vec.at(i));
  }
  return new_hits;
}

/***
ResortHitsByAPA comments:

Same as by window but into groupings by APA if the intial delivery of primitives is in groupings larger than one APA.
 ***/

vector<vector<unsigned int>> ResortHitsByAPA(vector<unsigned int> channel_vec, vector<unsigned int> sorting_vec){
  vector<vector<unsigned int>> new_hits(nAPAs);
  for (unsigned int i=0; i < channel_vec.size(); ++i){
    new_hits.at((int)channel_vec.at(i)/nChannels).push_back(sorting_vec.at(i));
  }
  return new_hits;

}

/***
ResortTPsByWindow comments:

Sorting a given vector of primitive characteristics (charge, time, channel) for any quantity of time into independent vectors of TPs into the desired windows. It also has applicability to sort non-channel sorted sets into channel-sorted sets. Will not return sub-ordering in time unless the input vector is already time-ordered.
***/

vector<vector<TP>> ResortTPsByWindow(vector<TP> TPs, int channel_sort){
  int index = -1;
  vector<vector<TP>> new_TPs(nticks/windows+1);
  if (channel_sort !=0){
    for (unsigned int i=0; i < TPs.size(); ++i){
      index = (int)TPs.at(i).tstart/windows;
      if (new_TPs.at(index).size()==0){
	new_TPs.at(index).push_back(TPs.at(i));
      }
      else if (TPs.at(i).channel >= new_TPs.at(index).back().channel){
	new_TPs.at(index).push_back(TPs.at(i));
      }
      else{
	for (unsigned int j=0; j < new_TPs.at(index).size(); ++j){
	  if (TPs.at(i).channel < new_TPs.at(index).at(j).channel){
	    new_TPs.at(index).insert(new_TPs.at(index).begin()+j,TPs.at(i));
	    break;
	  }
	}
      }
    }
  }
  else{
    for (unsigned int i=0; i < TPs.size(); ++i){
      index = (int)TPs.at(i).tstart/windows;
      new_TPs.at(index).push_back(TPs.at(i));
    }
  }
  return new_TPs;
}

/***
ResortTPsByAPA comments:

Same as by window but into groupings by APA if the intial delivery of primitives is in groupings larger than one APA.
 ***/

vector<vector<TP>> ResortTPsByAPA(vector<TP> TPs){
  vector<vector<TP>> new_TPs(nAPAs);
  for (unsigned int i=0; i < TPs.size(); ++i){
    new_TPs.at((int)TPs.at(i).channel/nChannels).push_back(TPs.at(i));
  }
  return new_TPs;
}

/***
ResortHitsByTPC comments:

This is the same idea as the above sorting functions.
It sorts by TPC such that you can ask for how much charge is on one side of the APA without having to ask which TPC it's in later.
Input sorting list should be a list that has already been sorted into APAs.
 ***/
vector<vector<unsigned int>> ResortHitsByTPC(vector<unsigned int> channel_vec, vector<unsigned int> sorting_vec, unsigned int APA){
  vector<vector<unsigned int>> new_hits(2);
  unsigned int APA_channel;
  for (unsigned int i=0; i < channel_vec.size(); ++i){
    APA_channel = channel_vec.at(i) - (nChannels*APA);
    if (APA_channel < 1600){
      cout << "SOMETHING IS SCREWED UP IN TPC SORTING. INDUCTION WIRE?" << endl;
      new_hits.clear();
      return new_hits;
    }
    else if ((int)(APA_channel) < (nChannels - nColl/2)){
      new_hits.at(0).push_back(sorting_vec.at(i));
    }
    else if ((int)(APA_channel) < nChannels){
      new_hits.at(1).push_back(sorting_vec.at(i));
    }
    else {
      cout << "SOMETHING IS SCREWED UP IN TPC SORTING. WRONG APA?" << endl;
      new_hits.clear();
      return new_hits;
    }
  }
  return new_hits;
}

/***
SortByTick comments:

The sorting necessary to be able to perform the Clustering algorithm as currently written. Sorts into 5us sub-windows.
 ***/
vector<vector<unsigned int>> SortByTick(vector<unsigned int> time_vec, vector<unsigned int> sorting_vec, unsigned int win){
  vector<vector<unsigned int>> new_hits(windows/prim_ticks); //never set prim_ticks such that windows is not a multiple of prim_ticks
  for (unsigned int i=0; i < time_vec.size(); ++i){
    new_hits.at(((int)time_vec.at(i)-windows*win)/prim_ticks).push_back(sorting_vec.at(i));
  }
  return new_hits;
}

// PRIMARY CLUSTERING FUNCTIONS

/***
AdjacentSameWindowCountingPrim:

Clustering code for the method called "Adjacency". This method assumes that the primitives are ordered in increasing channel order.
 ***/
int AdjacentSameWindowCountingPrim(vector<unsigned int> channels){
  int max = 0;
  int adj = 1;
  unsigned int channel = 0;
  unsigned int next_channel = 0;
  unsigned int next = 0;

  //Loop over the list of channels hit in a window.
  for (unsigned int i=0; i < channels.size(); ++i){
    next = (i+1)%channels.size();
    channel = channels.at(i);
    next_channel = channels.at(next);
    //Condition to deal with end of vector.
    if (next==0){
      next_channel=channel-1;
    }
    //Skip same channel hits since they are in order.
    if (next_channel == channel) continue;
    //If next hit is on next channel increase current adjacency.
    else if (next_channel == channel+1){
      ++adj;
    }
    // If next hit is definitely not part of a cluster of wires, check if current adjacency is the new maximum found in the window. If so, update maximum. Reset current adjacency.
    else{
      if(adj > max){
	max = adj;
      }
      adj = 1;
    }
  }
  return max;
}

/***
AdjacentSameWindowCountingPrimWithEverything:

This performs the above described "Adjacency" method (AdjacentSameWindowCountingPrim) while also checking the SummedAdcs for the primitives and the TOTs for the primitives. Returns Vector with the following:
0) Maximum adjacency in the window (as described in above)
1) Maximum single-primitive SummedADC
2) Maximum TPC Sum of SummedADC (whichever is the maximum of the two TPC sums of SummedADC)
3) Maximum single-primitive SummedADC
4) Sum total of TOTs in window
 ***/
vector<int> AdjacentSameWindowCountingPrimWithEverything(vector<unsigned int> channels, vector<unsigned int> adcs, vector<unsigned int> tots){
  vector<int> max(5,0);
  int adj = 1;
  int tpc_1_sum = 0;
  int tpc_2_sum = 0;
  unsigned int channel = 0;
  unsigned int next_channel = 0;
  unsigned int next = 0;
  for (unsigned int i=0; i < channels.size(); ++i){
    max.at(4) += tots.at(i);
    if ((int)(adcs.at(i)) > max.at(1)){
      max.at(1) = adcs.at(i);
    }
    if ((int)(tots.at(i)) > max.at(3)){
      max.at(3) = tots.at(i);
    }
    next = (i+1)%channels.size();
    channel = channels.at(i);
    next_channel = channels.at(next);
    if ((int)(channel%nChannels) < (nChannels-nColl/2))tpc_1_sum += adcs.at(i);
    else tpc_2_sum += adcs.at(i);
    if (next==0){
      next_channel=channel-1;
    }
    if (next_channel == channel) continue;
    else if (next_channel == channel+1){
      ++adj;
    }
    else{
      if(adj > max.at(0)){
        max.at(0) = adj;
      }
      adj = 1;
    }
  }
  if (tpc_1_sum > tpc_2_sum) max.at(2) = tpc_1_sum;
  else max.at(2) = tpc_2_sum;
  return max;
}

/***
PDUNEAdjacencyWithEverything:

This performs the above described "Adjacency" method (AdjacentSameWindowCountingPrim) while also checking the SummedAdcs for the primitives and the TOTs for the primitives. Returns Vector with the following (as will be useful in PDUNE DAQ test May 2019):
0) Maximum adjacency in the window (as described in above)
1) Maximum single-primitive SummedADC
2) Maximum TPC Sum of SummedADC (whichever is the maximum of the two TPC sums of SummedADC)
3) Maximum single-primitive SummedADC
4) First channel APA-wise of largest cluster
5) Last channel APA-wise of largest cluster
6) Tick associated with (4)
7) Tick associated with (5)

Relative to above, the sum total of all TOTs has been removed since we do not expect to use it as of right now.
***/
vector<int> PDUNEAdjacencyWithEverything(vector<unsigned int> channels, vector<unsigned int> times, vector<unsigned int> adcs, vector<unsigned int> tots){
  vector<int> max(8,0);
  int adj = 1;
  int tpc_1_sum = 0;
  int tpc_2_sum = 0;
  unsigned int channel = 0;
  unsigned int next_channel = 0;
  unsigned int next = 0;
  int first_channel = channels.at(0);
  int first_time = times.at(0);
  for (unsigned int i=0; i < channels.size(); ++i){
    if ((int)(adcs.at(i)) > max.at(1)){
      max.at(1) = adcs.at(i);
    }
    if ((int)(tots.at(i)) > max.at(3)){
      max.at(3) = tots.at(i);
    }
    next = (i+1)%channels.size();
    channel = channels.at(i);
    next_channel = channels.at(next);
    if ((int)(channel%nChannels) < (nChannels-nColl/2))tpc_1_sum += adcs.at(i);
    else tpc_2_sum += adcs.at(i);
    if (next==0){
      next_channel=channel-1;
    }
    if (next_channel == channel) continue;
    else if ((next_channel-channel) <= 2){
    //else if (next_channel == channel+1){
      adj += (next_channel-channel);
    }
    else{
      if(adj > max.at(0)){
        max.at(0) = adj;
	max.at(5) = (int)channel%nChannels-1600;
	max.at(7) = (int)times.at(i);
	max.at(4) = (int)first_channel%nChannels-1600;
	max.at(6) = (int)first_time;
      }
      first_channel = (int)next_channel;
      first_time = times.at(next);
      adj = 1;
    }
  }
  if (tpc_1_sum > tpc_2_sum) max.at(2) = tpc_1_sum;
  else max.at(2) = tpc_2_sum;
  return max;
}

/***
PDUNEAdjacency:

Same as PDUNEAdjacencyWithEverything, but handles the TP struct as opposed to the separate values
***/
vector<int> PDUNEAdjacency(vector<TP> TPs){
  vector<int> max(8,0);
  if(TPs.empty()) return max;
  int adj = 1;
  int tpc_1_sum = 0;
  int tpc_2_sum = 0;
  unsigned int channel = 0;
  unsigned int next_channel = 0;
  unsigned int next = 0;
  int first_channel = TPs.at(0).channel;
  int first_time = TPs.at(0).tstart;
  for (unsigned int i=0; i < TPs.size(); ++i){
    if ((int)(TPs.at(i).adcsum) > max.at(1)){
      max.at(1) = TPs.at(i).adcsum;
    }
    if ((int)(TPs.at(i).tspan) > max.at(3)){
      max.at(3) = TPs.at(i).tspan;
    }
    next = (i+1)%TPs.size();
    channel = TPs.at(i).channel;
    next_channel = TPs.at(next).channel;
    if ((int)(channel%nChannels) < (nChannels-nColl/2))tpc_1_sum += TPs.at(i).adcsum;
    else tpc_2_sum += TPs.at(i).adcsum;
    if (next==0){
      next_channel=channel-1;
    }
    if (next_channel == channel) continue;
    else if ((next_channel-channel) <= 2){
    //else if (next_channel == channel+1){
      adj += (next_channel-channel);
    }
    else{
      if(adj > max.at(0)){
        max.at(0) = adj;
	max.at(5) = (int)channel%nChannels-1600;
	max.at(7) = (int)TPs.at(i).tstart;
	max.at(4) = (int)first_channel%nChannels-1600;
	max.at(6) = (int)first_time;
      }
      first_channel = (int)next_channel;
      first_time = TPs.at(next).tstart;
      adj = 1;
    }
  }
  if (tpc_1_sum > tpc_2_sum) max.at(2) = tpc_1_sum;
  else max.at(2) = tpc_2_sum;
  return max;
}

/***
TimeAdjacentCounting:

Clustering code for the method called "Clustering". This method assumes that the primitives are ordered in increasing channel order. Returns the largest cluster size as chosen by the one with the largest charge in a sub-window or the largest sub-window adjacency, whichever is larger.
 ***/
int TimeAdjacentCounting(vector<vector<unsigned int>> channels, vector<vector<unsigned int>> adcs){
  vector<int> max(2,0);
  int maximum;
  int adj_prim = 1;
  int max_adj = 0;
  int adj = 1;
  float range = 1;
  float tmp_range = 0.0 ;
  float max_range = 0.0 ;
  float tmp_center = 0.0 ;
  float max_center = 0.0 ;
  float center = 400000000;
  unsigned int adc;
  unsigned int max_adc;
  unsigned int old = 0;
  unsigned int channel = 0;
  unsigned int last_channel = 0;

  //Loop over all sub-windows.
  for (unsigned int i=0;i < channels.size(); ++i){
    //cout << "Time: " << i << endl;
    //cout << "CURRENT cluster size: " << adj << endl;
    //cout << "Center at: " << center << " w/ Range: " << range << " from time: " << old << " at time: " << i << endl;

    //Check that the charge list and channel list are the same size. If not, something went wrong.
    if (channels.at(i).size() != adcs.at(i).size()){
      cout << "SORTING BROKE" << endl;
      max.at(0) = -1;
      max.at(1) = -1;
      break;
    }
    //Skip empty sub-windows
    if (channels.at(i).size() == 0) continue;

    //Simpler if there is only one hit in the sub-window.
    else if (channels.at(i).size() == 1){
      //      cout << "Single Hit on Channel: " << channels.at(i).at(0) << endl;
      // cout << "Window Tick: " << i << endl;
      // cout << "Channels: " << channels.at(i).at(0) << endl;

      //Only work in cases where the sub-window you are on is less than "too_far" away from the previous non-empty sub-window.
      //Generally, too_far is 2. Set above at start of code.
      if ((i-old) < too_far){

	//Check if the wire is adjacent to any of the wires in the previous sub-windows largest charge cluster.
	// If so increase current cluster size by one.
        if (fabs(channels.at(i).at(0)-center) <= range) ++adj;

	//If not,
        else{
	  // check if the current cluster size is larger than the maximum. If so, update maximum.
          if (adj > max.at(0)) max.at(0) = adj;
	  // check if maximum sub-window adjacency is less than current (of 1). If so, update maximum (to 1). 
          if (max.at(1) < 1) max.at(1) = 1;
	  //Reset cluster size.
          adj = 1;
        }
      }
      //Do Same checks if the case is such that the current sub-window you are on is more than "too_far" from the previous non-empty.
      else{
        if (adj > max.at(0)) max.at(0) = adj;
        if (max.at(1) < 1) max.at(1) = 1;
      }

      //Update variables necessary for checking whether clusters are connected.
      center = channels.at(i).at(0);
      range = 1;
      old = i;
    }

    //If there is more than one primitive in the sub-window.
    else{

      //cout << "Channels: ";

      adc = 0;
      max_center = 400000000;
      max_range = 1;
      max_adc = 0;
      max_adj = 0;
      channel = 400000000;
      last_channel = 400000000;

      //Perform "Adjacency" method within a sub-window. Save information for largest SummedADC cluster within the sub-window. Save largest sub-window adjacency.
      for (unsigned int j=0; j < channels.at(i).size(); ++j){

        last_channel = channel;
        channel = channels.at(i).at(j);

        //cout << "Channel: " << channel << endl;
        //cout << "Last Channel: " << last_channel << endl;

        if (channel == last_channel){
          adc += adcs.at(i).at(j);
          continue;
        }
        else if (channel == (last_channel+1)){
          //cout << "Upping Adj_Prim" << endl;
          adc += adcs.at(i).at(j);
          ++adj_prim;
          tmp_center +=0.5;
          tmp_range += 0.5;
        }
        else{
          if (adj_prim > max.at(1)) max.at(1) = adj_prim;
          if (adc > max_adc){
            max_adc = adc;
            max_center = tmp_center;
            max_range = tmp_range;
            max_adj = adj_prim;
          }
          tmp_center = channel;
          tmp_range = 1;
          adj_prim = 1;
          adc = adcs.at(i).at(j);
        }
        //cout << "Adj. Prim: " << adj_prim << endl;
      }

      //Check that the last sub-window adjacency is not the largest in either adjacency or summedADC. If it is, update variables accordingly.
      if (adj_prim > max.at(1)) max.at(1) = adj_prim;
      if (adc > max_adc){
        max_center = tmp_center;
        max_range = tmp_range;
        max_adj = adj_prim;
      }

      //Check if the largest SummedADC cluster of the present sub-window has any wires adjacent with any wires of the previous non-empty sub-window's largest cluster, iff the previous non-empty sub-window was closer than "too_far". Update maxima accordingly. Reset as needed.
      if ((i-old) < too_far){
        if (max_center == center){
          adj+=max_adj;
        }
        else if (max_center > center){
          if ((max_center+1-max_range) <= (center+range)) adj+=max_adj;
          else{
            if (adj > max.at(0)) max.at(0) = adj;
            adj = max_adj;
          }
        }
        else{
          if ((max_center-1+max_range) >= (center-range)){
            adj+=max_adj;
          }
          else{
            if (adj > max.at(0)) max.at(0) = adj;
            adj = max_adj;
          }
        }
      }

      else{
        if (adj > max.at(0)) max.at(0) = adj;
        adj = max_adj;
      }

      center = max_center;
      range = max_range;
      old = i;
    }
  }

  if (adj > max.at(0)) max.at(0) = adj;

  //cout << "TOT Adj.: " << max.at(0) << endl;
  //cout << "Prim. Adj.: " << max.at(1) << endl;

  if (max.at(0) >= max.at(1)) maximum = max.at(0);
  else maximum = max.at(1);
  return maximum;
}

/***
TimeAdjacentCountingWithEverything:

Performs "Clustering" (as described in TimeAdjacentCounting) while also checking SummedADCs and TOTs of the primitives. Returns a vector of the following:
0) Maximum Cluster Size for the window as described above (TimeAdjacentCounting)
1) Maximum single-primitive SummedADC 
2) Maximum TPC sum of Summed ADC (as described for AdjacentSameWindowCountingPrimWithEverything)
3) Maximum single-primitive TOT in the window
4) Total Sum of all TOTs in the window
 ***/
vector<int> TimeAdjacentCountingWithEverything(vector<vector<unsigned int>> channels, vector<vector<unsigned int>> adcs, vector<vector<unsigned int>> tots){
  vector<int> maxadj(2,0);
  vector<int> max(5,0);
  int adj_prim = 1;
  int max_adj = 0;
  int adj = 1;
  float range = 1;
  float tmp_range = 0.0;
  float max_range = 0.0;
  float tmp_center = 0.0;
  float max_center = 0.0;
  float center = 400000000;
  unsigned int adc;
  unsigned int max_adc;
  unsigned int old = 0;
  unsigned int channel = 0;
  unsigned int last_channel = 0;

  int tpc_1_sum = 0;
  int tpc_2_sum = 0;

  for (unsigned int i=0;i < channels.size(); ++i){
    //cout << "Time: " << i << endl;
    //cout << "CURRENT cluster size: " << adj << endl;
    //cout << "Center at: " << center << " w/ Range: " << range << " from time: " << old << " at time: " << i << endl;
    if (channels.at(i).size() != adcs.at(i).size() || channels.at(i).size() != tots.at(i).size() ){
      cout << "SORTING BROKE" << endl;
      maxadj.at(0) = -1;
      maxadj.at(1) = -1;
      break;
    }
    if (channels.at(i).size() == 0) continue;

    else if (channels.at(i).size() == 1){
      max.at(4) += tots.at(i).at(0);
      if ((int)(adcs.at(i).at(0)) > max.at(1)){
        max.at(1) = adcs.at(i).at(0);
      }
      if ((int)(tots.at(i).at(0)) > max.at(3)){
        max.at(3) = tots.at(i).at(0);
      }
      if ((int)(channels.at(i).at(0)%nChannels) < (nChannels-nColl/2))tpc_1_sum += adcs.at(i).at(0);
      else tpc_2_sum += adcs.at(i).at(0);
      //      cout << "Single Hit on Channel: " << channels.at(i).at(0) << endl;
      // cout << "Window Tick: " << i << endl;
      // cout << "Channels: " << channels.at(i).at(0) << endl;

      if ((i-old) < too_far){
        if (fabs(channels.at(i).at(0)-center) <= range) ++adj;
        else{
          if (adj > maxadj.at(0)) maxadj.at(0) = adj;
          if (maxadj.at(1) < 1) maxadj.at(1) = 1;
          adj = 1;
        }
      }
      else{
        if (adj > maxadj.at(0)) maxadj.at(0) = adj;
        if (maxadj.at(1) < 1) maxadj.at(1) = 1;
      }

      center = channels.at(i).at(0);
      range = 1;
      old = i;
    }
    else{

      //cout << "Channels: ";

      adc = 0;
      max_center = 400000000;
      max_range = 1;
      max_adc = 0;
      max_adj = 0;
      channel = 400000000;
      last_channel = 400000000;

      for (unsigned int j=0; j < channels.at(i).size(); ++j){
        max.at(4) += tots.at(i).at(j);
        if ((int)(adcs.at(i).at(j)) > max.at(1)){
          max.at(1) = adcs.at(i).at(j);
        }
        if ((int)(tots.at(i).at(j)) > max.at(3)){
          max.at(3) = tots.at(i).at(j);
        }

        last_channel = channel;
        channel = channels.at(i).at(j);
        if ((int)(channel%nChannels) < (nChannels-nColl/2))tpc_1_sum += adcs.at(i).at(j);
        else tpc_2_sum += adcs.at(i).at(j);

        //cout << "Channel: " << channel << endl;
        //cout << "Last Channel: " << last_channel << endl;

        if (channel == last_channel){
          adc += adcs.at(i).at(j);
          continue;
        }
        else if (channel == (last_channel+1)){
          //cout << "Upping Adj_Prim" << endl;
          adc += adcs.at(i).at(j);
          ++adj_prim;
          tmp_center +=0.5;
          tmp_range += 0.5;
        }
        else{
          if (adj_prim > maxadj.at(1)) maxadj.at(1) = adj_prim;
          if (adc > max_adc){
            max_adc = adc;
            max_center = tmp_center;
            max_range = tmp_range;
            max_adj = adj_prim;
          }
          tmp_center = channel;
          tmp_range = 1;
          adj_prim = 1;
          adc = adcs.at(i).at(j);
        }
        //cout << "Adj. Prim: " << adj_prim << endl;
      }

      if (adj_prim > maxadj.at(1)) maxadj.at(1) = adj_prim;
      if (adc > max_adc){
        max_center = tmp_center;
        max_range = tmp_range;
        max_adj = adj_prim;
      }

      if ((i-old) < too_far){
        if (max_center == center){
          adj+=max_adj;
        }
        else if (max_center > center){
          if ((max_center+1-max_range) <= (center+range)) adj+=max_adj;
          else{
            if (adj > maxadj.at(0)) maxadj.at(0) = adj;
            adj = max_adj;
          }
        }
        else{
          if ((max_center-1+max_range) >= (center-range)){
            adj+=max_adj;
          }
          else{
            if (adj > maxadj.at(0)) maxadj.at(0) = adj;
            adj = max_adj;
          }
        }
      }

      else{
        if (adj > maxadj.at(0)) maxadj.at(0) = adj;
        adj = max_adj;
      }

      center = max_center;
      range = max_range;
      old = i;
    }
  }

  if (adj > maxadj.at(0)) maxadj.at(0) = adj;

  //cout << "TOT Adj.: " << maxadj.at(0) << endl;
  //cout << "Prim. Adj.: " << maxadj.at(1) << endl;

  if (maxadj.at(0) >= maxadj.at(1)) max.at(0) = maxadj.at(0);
  else max.at(0) = maxadj.at(1);
  if (tpc_1_sum > tpc_2_sum) max.at(2) = tpc_1_sum;
  else max.at(2) = tpc_2_sum;
  return max;
}


//Rarely-used or deprecated functions

/***
WindowCounting comments:

NOT COMMONLY USED.
Input of all hits in an APA. Return maximum number in a single window (not necessarily adjacent).
 ***/
int WindowCounting(vector<unsigned int> time_vec){
  vector<int> window_vec(nticks/windows+1,0);
  for (unsigned int i=0; i < time_vec.size(); ++i){
    ++window_vec.at((int)time_vec.at(i)/windows);
  }
  int max = *max_element(window_vec.begin(),window_vec.end()); 
  return max;
}

//Functions whose functionality has been incorporated into combined clustering / finding maximums functions.

/***
FindMaxTOTCluster comments:

NOT COMMONLY USED.
Loop through vector of TOTs in a window. Return the largest single primitive TOT found in that window and the Largest sum of TOTs within a sub-window. 
 ***/
vector<unsigned int> FindMaxTOTCluster(vector<vector<unsigned int>> tots_per_window){
  vector<unsigned int> tots;
  vector<unsigned int> maxtot(2,0);
  unsigned int max = 0;
  unsigned int current = 0;
  unsigned int max_total = 0;
  unsigned int current_total = 0;
  for (unsigned int i=0; i < tots_per_window.size(); ++i){
    if (tots_per_window.at(i).size()==0)continue;
    tots = tots_per_window.at(i);
    current_total = 0;
    for (unsigned int j=0; j < tots.size(); ++j){
      current = tots.at(j);
      current_total += current;
      if (current > max){
	max = current;
      }
    }
    if (current_total > max_total){
      max_total = current_total;
    }
  }
  maxtot.at(0) = max;
  maxtot.at(1) = max_total;
  return maxtot;
}

/***
FindMaxSumADCCluster comments:

NOT COMMONLY USED.
Same as the above for TOT but for SummedADC (charge stand-in) of the primitives.
 ***/
vector<unsigned int> FindMaxSumADCCluster(vector<vector<unsigned int>> sum_adc_per_window){
  vector<unsigned int> sum_adc;
  vector<unsigned int> maxadc(2,0);
  unsigned int max = 0;
  unsigned int current = 0;
  unsigned int max_total = 0;
  unsigned int current_total = 0;
  for (unsigned int i=0; i < sum_adc_per_window.size(); ++i){
    if (sum_adc_per_window.at(i).size()==0)continue;
    sum_adc = sum_adc_per_window.at(i);
    current_total = 0;
    for (unsigned int j=0; j < sum_adc.size(); ++j){
      current = sum_adc.at(j);
      current_total += current;
      if (current > max){
	max = current;
      }
    }
    if (current_total > max_total){
      max_total = current_total;
    }
  }
  maxadc.at(0) = max;
  maxadc.at(1) = max_total;
  return maxadc;
}

/***
FindMaxTOTPrim comments:

NOT COMMONLY USED.
Loop through vector of TOTs in a window. Return the largest TOT found in that window and the average of the TOTs in that window.
 ***/
vector<unsigned int> FindMaxTOTPrim(vector<unsigned int> tots){
  vector<unsigned int> maxtot(2,0);
  unsigned int max = 0;
  unsigned int current = 0;
  unsigned int total = 0;
  unsigned int avg = 0;
  for (unsigned int j=0; j < tots.size(); ++j){
    current = tots.at(j);
    total += current;
    if (current > max){
      max = current;
    }
  }
  avg = total/(unsigned int)tots.size();
  maxtot.at(0) = max;
  maxtot.at(1) = avg;
  return maxtot;
}

/***
FindMaxSumADCPrim comments:

NOT COMMONLY USED.
Same as the above for TOT but for SummedADC (charge stand-in) of the primitives.
 ***/
vector<unsigned int> FindMaxSumADCPrim(vector<unsigned int> sum_adc_tpc_1, vector<unsigned int> sum_adc_tpc_2){
  vector<unsigned int> maxadc(3,0);
  unsigned int max = 0;
  unsigned int current = 0;
  unsigned int total_1 = 0;
  unsigned int total_2 = 0;
  //unsigned int total = 0;
  for (unsigned int j=0; j < sum_adc_tpc_1.size(); ++j){
    current = sum_adc_tpc_1.at(j);
    total_1 += current;
    if (current > max){
      max = current;
    }
  }
  for (unsigned int j=0; j < sum_adc_tpc_2.size(); ++j){
    current = sum_adc_tpc_2.at(j);
    total_2 += current;
    if (current > max){
      max = current;
    }
  }
  maxadc.at(0) = max;
  //maxadc.at(1) = total;

  if (total_1 > total_2){
    maxadc.at(1) = total_1;
    maxadc.at(2) = 1;
  }
  else if (total_1 < total_2){
    maxadc.at(1) = total_2;
    maxadc.at(2) = 2;
  }
  else{
    maxadc.at(1) = total_1;
    maxadc.at(2) = 3;
  }

  return maxadc;
}

/***
RightAdjacentLaterWindowCounting Comments:

Function that clusters across sub-windows and up one in wire. Replaced by more complex "Clustering".
 ***/
int RightAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window){
  vector<vector<unsigned int>> channels(channels_per_window);
  int max = 0;
  int adj = 1;
  unsigned int channel = 0;
  unsigned int next_channel = 0;
  unsigned int next_window = 0;
  unsigned int next_ind = 0;
  for (unsigned int i=0; i < channels.size(); ++i){
    if (channels.at(i).size() == 0) continue;
    if (i == (channels.size()-1)){
      if(adj > max){
	max = adj;
      }
      continue;
    }
    for (unsigned int j=0; j < channels.at(i).size();++j){
      next_window = i+1;
      next_ind = 0;
      if (channels.at(next_window).size() == 0){
	if(adj > max){
	  max = adj;
	}
	break;
      }
      channel = channels.at(i).at(j);
      while (next_window < channels.size()){
	if (channels.at(next_window).size() == 0) break;
	next_channel = channels.at(next_window).at(next_ind);
	if ((int)(next_channel - channel) > (int)(next_window - i)) break;
	else if ((int)(next_channel - channel) < (int)(next_window - i)){
	  ++next_ind;
	  if (next_ind > (channels.at(next_window).size()-1)) break;
	}
	else{
	  ++adj;
	  channels.at(next_window).erase(channels.at(next_window).begin()+next_ind);
	  ++next_window;
	  next_ind = 0;
	}
      }
      if(adj > max){
	max = adj;
      }
      adj = 1;
    }
  }
  return max;
}

/***
SameAdjacentLaterWindowCounting Comments:

Function that clusters across sub-windows on the same wire. Replaced by more complex "Clustering".
 ***/
int SameAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window){
  vector<vector<unsigned int>> channels(channels_per_window);
  int max = 0;
  int adj = 1;
  int channel = 0;
  int next_channel = 0;
  unsigned int next_window = 0;
  unsigned int next_ind = 0;
  for (unsigned int i=0; i < channels.size(); ++i){
    if (channels.at(i).size() == 0) continue;
    if (i == (channels.size()-1)){
      if(adj > max){
	max = adj;
      }
      continue;
    }
    for (unsigned int j=0; j < channels.at(i).size();++j){
      next_window = i+1;
      next_ind = 0;
      if (channels.at(next_window).size() == 0){
	if(adj > max){
	  max = adj;
	}
	break;
      }
      channel = channels.at(i).at(j);
      while (next_window < channels.size()){
	if (channels.at(next_window).size() == 0) break;
	next_channel = channels.at(next_window).at(next_ind);
	if (channel < next_channel) break;
	else if (channel  >  next_channel){
	  ++next_ind;
	  if (next_ind > (channels.at(next_window).size()-1)) break;
	}
	else{
	  ++adj;
	  channels.at(next_window).erase(channels.at(next_window).begin()+next_ind);
	  ++next_window;
	  next_ind = 0;
	}
      }
      if(adj > max){
	max = adj;
      }
      adj = 1;
    }
  }
  return max;
}

/***
LeftAdjacentLaterWindowCounting Comments:

Function that clusters across sub-windows and down one in wire. Replaced by more complex "Clustering".
 ***/
int LeftAdjacentLaterWindowCounting(vector<vector<unsigned int>> channels_per_window){
  vector<vector<unsigned int>> channels(channels_per_window);
  int max = 0;
  int adj = 1;
  unsigned int channel = 0;
  unsigned int next_channel = 0;
  unsigned int next_window = 0;
  unsigned int next_ind = 0;
  for (unsigned int i=0; i < channels.size(); ++i){
    if (channels.at(i).size() == 0) continue;
    if (i == (channels.size()-1)){
      if(adj > max){
	max = adj;
      }
      continue;
    }
    for (unsigned int j=0; j < channels.at(i).size();++j){
      next_window = i+1;
      next_ind = 0;
      if (channels.at(next_window).size() == 0){
	if(adj > max){
	  max = adj;
	}
	break;
      }
      channel = channels.at(i).at(j);
      while (next_window < channels.size()){
	if (channels.at(next_window).size() == 0) break;
	next_channel = channels.at(next_window).at(next_ind);
	if ((int)(channel - next_channel) < (int)(next_window - i)) break;
	else if ((int)(channel - next_channel) > (int)(next_window - i)){
	  ++next_ind;
	  if (next_ind > (channels.at(next_window).size()-1)) break;
	}
	else{
	  ++adj;
	  channels.at(next_window).erase(channels.at(next_window).begin()+next_ind);
	  ++next_window;
	  next_ind = 0;
	}
      }
      if(adj > max){
	max = adj;
      }
      adj = 1;
    }
  }
  return max;
}

/***
LeftAdjacentLaterWindowCounting Comments:

Function that clusters within sub-windows. Replaced by more complex "Clustering".
 ***/
int AdjacentSameWindowCountingCluster(vector<vector<unsigned int>> channels_per_window){
  int max = 0;
  int adj = 1;
  unsigned int channel = 0;
  unsigned int next_channel = 0;
  unsigned int next = 0;
  vector<unsigned int> channels;
  for (unsigned int it=0; it < channels_per_window.size(); ++it){
    if (channels_per_window.at(it).size()==0) continue;
    channels = channels_per_window.at(it);
    for (unsigned int i=0; i < channels.size(); ++i){
      next = (i+1)%channels.size();
      channel = channels.at(i);
      next_channel = channels.at(next);
      if (next==0){
	next_channel=channel-1;
      }
      if (next_channel == channel) continue;
      else if (next_channel == channel+1){
	++adj;
      }
      else{
	if(adj > max){
	  max = adj;
	}
	adj = 1;
      }
    }
  }
  return max;
}
