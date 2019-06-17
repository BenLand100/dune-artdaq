
/*************************************************************************
    > File Name: makeMap.cc
    > Author: Jingbo Wang
    > Mail: jiowang@ucdavis.edu 
    > Created Time: Tue Jan 23 13:57:14 2018
 ************************************************************************/

using namespace std;

// Implementation of Table 5 in DocDB 4064: FEMB channel (chipNo & chipChannel) to plane type
int plane[8][16] = {
										 {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
										 {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2}, 
										 {2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
										 {2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
										 {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
										 {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
										 {2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
										 {2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0}
	                 };

// Implementation of Table 5 in DocDB 4064: FEMB channel (chipNo & chipChannel) to plane channel number                 
int planeCh[8][16] = {
										 {19, 17, 15, 13, 11, 19, 17, 15, 13, 11, 23, 21, 19, 17, 15, 13},
										 {9,  7,  5,  3,  1,  9,  7,  5,  3,  1,  11, 9,  7,  5,  3,  1}, 
										 {14, 16, 18, 20, 22, 24, 12, 14, 16, 18, 20, 12, 14, 16, 18, 20},
										 {2,  4,  6,  8,  10, 12, 2,  4,  6,  8,  10, 2,  4,  6,  8,  10},
										 {29, 27, 25, 23, 21, 29, 27, 25, 23, 21, 35, 33, 31, 29, 27, 25},
										 {39, 37, 35, 33, 31, 39, 37, 35, 33, 31, 47, 45, 43, 41, 39, 37},
										 {26, 28, 30, 32, 34, 36, 22, 24, 26, 28, 30, 22, 24, 26, 28, 30},
										 {38, 40 ,42, 44, 46, 48, 32, 34, 36, 38, 40, 32, 34, 36, 38, 40}
	                 };

// Implementation of SLAC's channel map: FEMB channel (chipNo & chipChannel) to RCE channel number                 
int rceCh[8][16] = {
										 {14, 12, 10, 8,  6,  4,  2,  0,  30, 28, 26, 24, 22, 20, 18, 16},
										 {46, 44, 42, 40, 38, 36, 34, 32, 62, 60, 58, 56, 54, 52, 50, 48},
										 {15, 13, 11, 9,  7,  5,  3,  1,  31, 29, 27, 25, 23, 21, 19, 17},
										 {47, 45, 43, 41, 39, 37, 35, 33, 63, 61, 59, 57, 55, 53, 51, 49},
										 {78, 76, 74, 72, 70, 68, 66, 64, 94, 92, 90, 88, 86, 84, 82, 80},
										 {110, 108, 106, 104, 102, 100, 98, 96, 126, 124, 122, 120, 118, 116, 114, 112},
										 {79,  77,  75,  73,  71,  69,  67, 65, 95,  93,  91,  89,  87,  85,  83,  81},
										 {111, 109, 107, 105, 103, 101, 99, 97, 127, 125, 123, 121, 119, 117, 115, 113}
	                 };

void MakePdspChannelMap_v3() {
 
  ofstream fmap;
  fmap.open ("protoDUNETPCChannelMap_v3.txt");
  int nCrate = 6;
  int nSlotPerCrate = 5;
  int nFiberPerSlot = 4;
  int nChipPerFEMB = 8;
  int nASICPerChip = 2;
  int nChannelPerASIC = 8;
  int FEMBChannel = 0;
  int StreamChannel = 0;
  int chipChannel = 0;
  int asicChannel = 0;
  int slotID = 0;
  int fiberID = 0;
  
  int Asic = 0;
  int offlineChannel  = 0;
  for(int crateNo = 0; crateNo<nCrate; crateNo++) {
  	int planeType = 0;
  	vector<int> Uwire, Vwire, Zwire;
     for(int slotNo = 0;slotNo<nSlotPerCrate;slotNo++) {
     	 for(int fiberNo = 0;fiberNo<nFiberPerSlot;fiberNo++) {
     	 	 FEMBChannel = 0;
     	 	 StreamChannel = 0;
     	 	 for(int chipNo = 0;chipNo<nChipPerFEMB;chipNo++) {  
     	 	 	 chipChannel =0; 	 	 	 
     	 	 	 for(int asicNo = 0;asicNo<nASICPerChip;asicNo++) {
     	 	 	 	 asicChannel = 0;
     	 	 	 	 for(int chNo = 0; chNo<nChannelPerASIC; chNo++) {
     	 	 	 	 	 //plane type
     	 	 	 	 	 planeType =  plane[chipNo][chipChannel];
     	 	 	 	 	 
     	 	 	 	 	 //RCE/FELIX channel number
     	 	 	 	 	 StreamChannel = rceCh[chipNo][chipChannel]; // FIXME: FELIX needs its own map
     	 	        	
     	 	       //offline number
     	 	       if(planeType == 0) {
     	 	       	 Uwire.push_back(planeCh[chipNo][chipChannel]);
     	 	       	 offlineChannel = 2560*crateNo + 40*(4*slotNo + fiberNo)+ (planeCh[chipNo][chipChannel]-1);
     	 	       }
     	 	       if(planeType == 1) {
     	 	       	 Vwire.push_back(planeCh[chipNo][chipChannel]);
     	 	       	 offlineChannel = 2560*crateNo +  800 + 40*(4*slotNo + fiberNo) + (planeCh[chipNo][chipChannel]-1);
     	 	       }
     	 	       if(planeType == 2) {
     	 	       	 Uwire.push_back(planeCh[chipNo][chipChannel]);
     	 	       	 offlineChannel = 2560*crateNo + 1600 + 48*(4*slotNo + fiberNo)  + (planeCh[chipNo][chipChannel]-1);
     	 	       }
     	 	       
     	 	       
     	 	 	 	 	 fmap<<crateNo<<"\t"<<slotNo<<"\t"<<fiberNo<<"\t"<<FEMBChannel
     	 	 	 	 	     <<"\t"<<StreamChannel<<"\t"<<slotID<<"\t"<<fiberID
     	 	 	 	 	     <<"\t"<<chipNo<<"\t"<<chipChannel<<"\t"<<asicNo<<"\t"
     	 	 	 	 	     <<asicChannel<<"\t"<<planeType<<"\t"<<offlineChannel<<endl;  
     	 	 	 	 	 FEMBChannel++;
     	 	 	 	 	 chipChannel++;
     	 	 	 	 	 asicChannel++;
//     	 	       offlineChannel++; 	 	
     	 	 	 	 }
     	 	 	 }    	 	   
     	 	 } 
     	 	 fiberID++; 
     	 }
     	 slotID++;
     }
  }
  fmap.close();
  
}
