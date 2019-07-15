#include <iostream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

std::vector<int> TriggerCandidate(std::vector<TP>, int clustering);
std::vector<int> TriggerCandidateHits(std::vector<unsigned int> channels,
                                      std::vector<unsigned int> times,
                                      std::vector<unsigned int> tots,
                                      std::vector<unsigned int> adcs,
                                      int clustering);
