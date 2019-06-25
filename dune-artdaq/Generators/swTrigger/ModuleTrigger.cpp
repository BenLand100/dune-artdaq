#include "ModuleTrigger.h"

//convenient for us! let's not bother with std namespaces!
using namespace std;

/*
The following function takes some set of trigger candidates as designed for the May 2019 protoDUNE DAQ test to look for horizontal crossing muons, keeping in mind at the time of writing, only APA 1 and 3 (by LArSoft's numbering) were instrumented with Felix.
*/

int nAPAs_Felix = 2;

int window_time_gap = 2;
int window_channel_gap = 2;
int APA_time_gap = 10;
int APA_channel_gap = 1;
int size_thresh = 450;

double slope_diff = 0.1;

int ModuleTrigger(vector<vector<int>> candidates){
  //PERFORM ANY SORTING NECESSARY, RIGHT NOW, NONE DONE SINCE PACKAGE STRUCTURE PRESENTLY UNCLEAR AND ASSUMED TO BE TIME ORDERED.

  int last_end_time = -1000;
  int last_end_channel = -1000;
  int last_APA = -1000;

  int trigger;

  vector<int> APA_size(nAPAs_Felix,0);

  double last_slope=-1000;
  double slope=-1000;

  for (unsigned int i=0; i < candidates.size(); ++i){

    /*
    cout << "CANDIDATE" << endl;
    cout << "APA: " << candidates.at(i).at(9) << endl;
    cout << "Window: " << candidates.at(i).at(10) << endl;
    cout << "ADJ: " << candidates.at(i).at(0) << endl;
    //    cout << "First Channel: " << candidates.at(i).at(4) << endl;
    cout << "First Tick: " << candidates.at(i).at(6) << endl;
    //cout << "Last Channel: " << candidates.at(i).at(5) << endl;
    cout << "Last Tick: " << candidates.at(i).at(7) << endl;
/    */

    slope = (double)(candidates.at(i).at(6)-candidates.at(i).at(7))/(double)(candidates.at(i).at(4)-candidates.at(i).at(5));

    /*
    cout << "Slope: " << slope << endl;
    cout << "" << endl;
    */

    if (candidates.at(i).at(8) == last_APA){
      if (last_slope < 0){
	if (fabs(candidates.at(i).at(5)-last_end_channel) <= window_channel_gap && 
	    fabs(candidates.at(i).at(7)-last_end_time) <= window_time_gap &&
	    fabs(slope-last_slope) <= slope_diff){
	  APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	}
	else{
	  for (int j=0; j < nAPAs_Felix; ++j){
	    if (APA_size.at(j) >= size_thresh){
	      ++trigger;
	    }
	    APA_size.at(j)=0;
	  }
	  if (trigger == nAPAs_Felix){
	    return 1;
	  }
	  else{
	    trigger=0;
	    APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	  }
	}
      }
      else if (last_slope > 0){
	if (fabs(candidates.at(i).at(4)-last_end_channel) <= window_channel_gap && 
	    fabs(candidates.at(i).at(6)-last_end_time) <= window_time_gap &&
	    fabs(slope-last_slope) <= slope_diff){
	  APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	}
	else{
	  for (int j=0; j < nAPAs_Felix; ++j){
	    if (APA_size.at(j) >= size_thresh){
	      ++trigger;
	    }
	    APA_size.at(j)=0;
	  }
	  if (trigger == nAPAs_Felix){
	    return 1;
	  }
	  else{
	    trigger=0;
	    APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	  }
	}
      }
      else{
	//TOO LAZY TO CODE ANYTHING FOR 0 SLOPE RN... IT'S TRICKY...	
      }
    }

    else if (fabs(candidates.at(i).at(8)-last_APA) == 1){
      if (last_slope < 0){
	if (fabs(479-candidates.at(i).at(5)+last_end_channel) <= APA_channel_gap && 
	    fabs(candidates.at(i).at(7)-last_end_time) <= APA_time_gap &&
	    fabs(slope-last_slope) <= slope_diff){
	  APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	}
	else{
	  for (int j=0; j < nAPAs_Felix; ++j){
	    if (APA_size.at(j) >= size_thresh){
	      ++trigger;
	    }
	    APA_size.at(j)=0;
	  }
	  if (trigger == nAPAs_Felix){
	    return 1;
	  }
	  else{
	    trigger=0;
	    APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	  }
	}
      }
      else if (last_slope > 0){
	if (fabs(479+candidates.at(i).at(4)-last_end_channel) <= APA_channel_gap && 
	    fabs(candidates.at(i).at(6)-last_end_time) <= APA_time_gap &&
	    fabs(slope-last_slope) <= slope_diff){
	  APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	}
	else{
	  for (int j=0; j < nAPAs_Felix; ++j){
	    if (APA_size.at(j) >= size_thresh){
	      ++trigger;
	    }
	    APA_size.at(j)=0;
	  }
	  if (trigger == nAPAs_Felix){
	    return 1;
	  }
	  else{
	    trigger=0;
	    APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
	  }
	}
      }
      else{
	//TOO LAZY TO CODE ANYTHING FOR 0 SLOPE RN... IT'S TRICKY...
      }
    }

    else{
      for (int j=0; j < nAPAs_Felix; ++j){
	if (APA_size.at(j) >= size_thresh){
	  ++trigger;
	}
	APA_size.at(j)=0;
      }
      if (trigger == nAPAs_Felix){
	return 1;
      }
      else{
	trigger=0;
	APA_size.at(candidates.at(i).at(8))+=candidates.at(i).at(0);
      }
    }


    last_slope = slope;
    last_APA = candidates.at(i).at(8);
    if (slope < 0){
      last_end_channel = candidates.at(i).at(4);
      last_end_time = candidates.at(i).at(6);
    }
    else if (slope > 0){
      last_end_channel = candidates.at(i).at(5);
      last_end_time = candidates.at(i).at(7);
    }
    else{
      //TOO LAZY TO CODE ANYTHING FOR 0 SLOPE RN... IT'S TRICKY...
    }
  }
  for (int j=0; j < nAPAs_Felix; ++j){
    if (APA_size.at(j) >= size_thresh){
      ++trigger;
    }
    APA_size.at(j)=0;
  }
  if (trigger == nAPAs_Felix){
    return 1;
  }
  else{
    return 0;
  }
}
