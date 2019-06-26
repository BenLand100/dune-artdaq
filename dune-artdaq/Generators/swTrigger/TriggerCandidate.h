#include <iostream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;

vector<int> TriggerCandidate(vector<TP>, int clustering=0);
vector<int> TriggerCandidateHits(vector<unsigned int> channels, vector<unsigned int> times, vector<unsigned int> tots, vector<unsigned int> adcs, int clustering=0);
