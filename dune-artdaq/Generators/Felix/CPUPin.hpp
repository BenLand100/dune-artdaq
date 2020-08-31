#ifndef CPU_PIN_HPP
#define CPU_PIN_HPP

#include <iostream>
#include <fstream>
#include <sstream>

#include <sched.h>

#include "json.hpp"


#define DEBUG_CPU_PIN

class CPUPin
{
public:

  static CPUPin& getInstance(){
    static CPUPin myInstance;
    return myInstance;
  }
  CPUPin(CPUPin const&) = delete;             // Copy construct
  CPUPin(CPUPin&&) = delete;                  // Move construct
  CPUPin& operator=(CPUPin const&) = delete;  // Copy assign
  CPUPin& operator=(CPUPin &&) = delete;      // Move assign 

  cpu_set_t getReaderCPUs(unsigned flxId, unsigned cardId) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    auto& cpuVec = m_cardreaderCores[flxId][cardId];
    for (auto cpu : cpuVec){
      CPU_SET(cpu, &cpuset);
    }
    return cpuset;
  }

  cpu_set_t getDispatcherCPUs(unsigned flxId, unsigned cardId, unsigned linkId) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    auto& cpuVec = m_dispatcherCores[flxId][cardId][linkId];
    for (auto cpu : cpuVec){
      CPU_SET(cpu, &cpuset);
    }
    return cpuset;
  }

  cpu_set_t getSelectorCPUs(unsigned linkId) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    auto& cpuVec = m_selectorCores[linkId];
    for (auto cpu : cpuVec){
      std::cout << " CHANGED! PROVIDING CPU: " << cpu << " FOR LINKID: " << linkId << '\n';
      CPU_SET(cpu, &cpuset);  
    }
    return cpuset;
  }

  bool load(const std::string& filename) {
    std::ifstream i(filename);
    i >> m_pinsJson;
    for (auto it = m_pinsJson.begin(); it != m_pinsJson.end(); ++it) {
      std::istringstream ciss(it.key());
      unsigned int cardId;
      ciss >> cardId;
      for (auto cit = it.value()["cards"].begin(); cit!=it.value()["cards"].end(); ++cit) {
        std::istringstream chiss(cit.key());
        unsigned int cardPartId;
        chiss >> cardPartId;
        std::vector<unsigned> readerCPUs = cit.value()["reader"];
        m_cardreaderCores[cardId][cardPartId] = readerCPUs;
        for (auto lit = cit.value()["links"].begin(); lit!=cit.value()["links"].end(); ++lit) {
          std::istringstream liss(lit.key());
          unsigned linkId;
          liss >> linkId;
          std::vector<unsigned> linkCPUs = lit.value();
          m_dispatcherCores[cardId][cardPartId][linkId] = linkCPUs;
        } // links
      } // card halfs
      for (auto sit = it.value()["selectors"].begin(); sit!=it.value()["selectors"].end(); ++sit){
        std::istringstream shiss(sit.key());
        unsigned selId;
        shiss >> selId;
        std::vector<unsigned> selCPUs = sit.value();
        m_selectorCores[selId] = selCPUs;
      }
      auto selJson = it.value()["selectors"];
      
    } // card types

#ifdef DEBUG_CPU_PIN
    std::cout << "CARDREADERS: \n";
    for (auto it=m_cardreaderCores.begin(); it!=m_cardreaderCores.end(); ++it){
      unsigned cid = it->first;
      std::cout << "  CARD " << cid << '\n'; 
      for (auto it2=it->second.begin(); it2!=it->second.end(); ++it2) {
        unsigned hid = it2->first;
        std::cout << "  -> READER " << it2->first << " pinned to: "; 
        for (auto cpu : it2->second){
          std::cout << cpu << " ";
        }
        std::cout << '\n';
        auto disps = m_dispatcherCores[cid][hid];
        for (auto linkIt=disps.begin(); linkIt!=disps.end(); ++linkIt){
          unsigned lid = linkIt->first;
          std::cout << "    -> LINK " << lid << " pinned to: ";
          for (auto lcpu : linkIt->second){
            std::cout << lcpu << " ";
          }
          std::cout << '\n';
        }//links 
      }//cardreaders
    }//cards
    std::cout << "SELECTORS: \n";
    for (auto linkIt = m_selectorCores.begin(); linkIt!=m_selectorCores.end(); ++linkIt){
      std::cout << "   -> LINK " << linkIt->first << " pinned to: "; 
      for (auto lcpu : linkIt->second){
        std::cout << lcpu << " ";
      }
      std::cout << '\n';
    }
#endif
    return true;
  }; 


protected:
  // Singleton mode
  CPUPin() { };
  ~CPUPin() { };

private:

  nlohmann::json m_pinsJson;

  // 711 -> card0,card1 CardReaders-> vector of cores to pin.
  std::map<unsigned, std::map<unsigned, std::vector<unsigned>>> m_cardreaderCores;
  // 711 -> card0,card1 -> linkId Dispatchers -> vector of cores to pin.
  std::map<unsigned, std::map<unsigned, std::map<unsigned, std::vector<unsigned>>>> m_dispatcherCores;
  // 711 -> trm -> linkId to process -> vector of cores to pin.
  std::map<unsigned, std::vector<unsigned>> m_selectorCores;

};

#endif // CPU_PIN_HPP

