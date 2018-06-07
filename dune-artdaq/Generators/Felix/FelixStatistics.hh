#ifndef FELIX_STATISTICS_HH_
#define FELIX_STATISTICS_HH_

#include <thread>
#include <iostream>
#include <vector>
#include <bitset>
#include <string>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)


/*
 * NetioHandler
 * Author: Roland.Sipos@cern.ch
 * Description: FELIX Metric Publisher 
 * Date: November 2017
*/
class FelixStatistics {

public:

  /* Explicitly using the default constructor to
  *  underline the fact that it does get called. */
  FelixStatistics(netio::subscribe_socket& subscribe_socket,
             folly::ProducerConsumerQueue<netio::message>& producer_consumer_queue)
    : worker_thread(), sub_socket{ subscribe_socket }, pcq{ producer_consumer_queue }
  { 
  }

  ~FelixStatistics() {
    stop_thread = true;
    if (worker_thread.joinable())
      worker_thread.join();
  }

  // This may be a bad idea! Should not move at all?
  // Or this is good, just passing the refs?
  // Otherwise force delete: FelixStatistics(FelixStatistics&& other) = delete;
  FelixStatistics(FelixStatistics&& other) //noexcept {
    : sub_socket( std::ref(other.sub_socket) ),
      pcq( std::ref(other.pcq) ) {
  }

  // Actually start the thread. Notice Move semantics!
  void publishMetrics() {
    whatAmI = "metric_publisher";
    std::cout << "WOOF -> FelixStatistics:: moved to become a metric_publisher...\n";
    worker_thread = std::thread(&FelixStatistics::PublishMetrics, this);
  }

  void join() {
    stop_thread = true;
    if (worker_thread.joinable()) {
      worker_thread.join();
    }
  }

private:
  // Actual thread handler members.
  std::string whatAmI = "blank";
  std::thread worker_thread;
  std::atomic_bool stop_thread{ false };

  // References coming from the HWInterface.
  //netio::subscribe_socket& sub_socket;
  //folly::ProducerConsumerQueue<netio::message>& pcq;


  // The main working thread.
  void PublishMetrics() {  
    while(!stop_thread) {
      std::cout << "WOOF -> Stats should send metrics! \n";
      std::this_thread::sleep_for(std::seconds(5));
    }
  }

};

#endif /* FELIX_STATISTICS_HH_ */

