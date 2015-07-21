/*
 * PennDataReceiver.hh
 *
 *  Created on: Dec 15, 2014
 *      Author: tdealtry (based on tcn45 rce code)
 */

#ifndef PENNDATARECEIVER_HH_
#define PENNDATARECEIVER_HH_

#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>

#include "lbne-artdaq/Generators/RceSupportLib/SafeQueue.hh"
#include "lbne-artdaq/Generators/pennBoard/PennRawBuffer.hh"
#include "lbne-raw-data/Overlays/PennMicroSlice.hh"
#include "lbne-artdaq/Generators/pennBoard/PennCompileOptions.hh"
#include "messagefacility/MessageLogger/MessageLogger.h"

using boost::asio::ip::tcp;

namespace lbne {

class PennDataReceiver {

public:
  PennDataReceiver(int debug_level, uint32_t tick_period_usecs, uint16_t udp_receive_port, uint32_t millislice_size, uint16_t millislice_overlap_size, bool rate_test);
	virtual ~PennDataReceiver();

	void start();
	void stop();

	void commit_empty_buffer(PennRawBufferPtr& buffer);
	size_t empty_buffers_available(void);
	size_t filled_buffers_available(void);
	bool retrieve_filled_buffer(PennRawBufferPtr& buffer, unsigned int timeout_us=0);
	void release_empty_buffers(void);
	void release_filled_buffers(void);

private:

	enum DeadlineIoObject { None, Acceptor, DataSocket };

	void run_service(void);
	void do_accept(void);
	void do_read(void);
	void handle_received_data(std::size_t length);
	void suspend_readout(bool await_restart);

	void set_deadline(DeadlineIoObject io_object, unsigned int timeout_us);
	void check_deadline(void);

	int debug_level_;

	boost::asio::io_service io_service_;
	tcp::acceptor           acceptor_;
	tcp::socket	        accept_socket_;
	tcp::socket		data_socket_;

	boost::asio::deadline_timer deadline_;
	DeadlineIoObject deadline_io_object_;
	uint32_t tick_period_usecs_;

	uint16_t receive_port_;
  uint32_t millislice_size_;

	std::atomic<bool> run_receiver_;
	std::atomic<bool> suspend_readout_;
	std::atomic<bool> readout_suspended_;

	int recv_socket_;

	std::unique_ptr<std::thread> receiver_thread_;

	SafeQueue<lbne::PennRawBufferPtr> empty_buffer_queue_;
	SafeQueue<lbne::PennRawBufferPtr> filled_buffer_queue_;
	PennRawBufferPtr current_raw_buffer_;
	void*            current_write_ptr_;

  void*            state_start_ptr_;
  std::size_t      state_nbytes_recvd_;

  enum NextReceiveState { ReceiveMicrosliceHeader, ReceiveMicroslicePayload, ReceiveMicroslicePayloadHeader, ReceiveMicroslicePayloadCounter, ReceiveMicroslicePayloadTrigger, ReceiveMicroslicePayloadTimestamp };
  std::string nextReceiveStateToString(NextReceiveState val);
  std::vector<std::string> const next_receive_state_names_ 
  { "ReceiveMicrosliceHeader", "ReceiveMicroslicePayload", "ReceiveMicroslicePayloadHeader", "ReceiveMicroslicePayloadCounter", "ReceiveMicroslicePayloadTrigger", "ReceiveMicroslicePayloadTimestamp" };
  NextReceiveState next_receive_state_;
  size_t           next_receive_size_;
  size_t           nextReceiveStateToExpectedBytes(NextReceiveState val);

  enum MillisliceState { MillisliceEmpty, MillisliceIncomplete, MicrosliceIncomplete, MillisliceComplete };
  std::string millisliceStateToString(MillisliceState val);
  std::vector<std::string> const millislice_state_names_ 
  { "MillisliceEmpty", "MillisliceIncomplete", "MicrosliceIncomplete", "MillisliceComplete" };
  MillisliceState  millislice_state_;
  size_t           millislice_size_recvd_;
  uint16_t         microslices_recvd_;
  uint16_t         payloads_recvd_;
  uint16_t         payloads_recvd_counter_;
  uint16_t         payloads_recvd_trigger_;
  uint16_t         payloads_recvd_timestamp_;
  uint16_t         payloads_recvd_selftest_;
  uint16_t         payloads_recvd_checksum_;
  lbne::PennMicroSlice::Header::block_size_t microslice_size_;
  size_t           microslice_size_recvd_;
  uint32_t         millislices_recvd_;
  
  uint16_t microslices_recvd_timestamp_; //counts the number of microslices received that contain a timestamp word
  bool microslice_seen_timestamp_word_;
  bool last_microslice_was_fragment_;

  bool                                           microslice_version_initialised_;
  lbne::PennMicroSlice::Header::format_version_t last_microslice_version_;
  bool                                           sequence_id_initialised_;
  lbne::PennMicroSlice::Header::sequence_id_t    last_sequence_id_;

	std::chrono::high_resolution_clock::time_point start_time_;

  bool rate_test_;

  size_t           overlap_size_;
  static const int overlap_buffer_size_ = 65536;
  uint8_t          overlap_ptr_[lbne::PennDataReceiver::overlap_buffer_size_];
  uint16_t         millislice_overlap_size_;

  lbne::PennMicroSlice::sample_count_t overlap_payloads_recvd_;
  lbne::PennMicroSlice::sample_count_t overlap_payloads_recvd_counter_;
  lbne::PennMicroSlice::sample_count_t overlap_payloads_recvd_trigger_;
  lbne::PennMicroSlice::sample_count_t overlap_payloads_recvd_timestamp_;
  lbne::PennMicroSlice::sample_count_t overlap_payloads_recvd_selftest_;
  lbne::PennMicroSlice::sample_count_t overlap_payloads_recvd_checksum_;

  size_t           current_microslice_;
  static const int current_microslice_buffer_size_ = 65536;
  uint8_t          current_microslice_ptr_[lbne::PennDataReceiver::current_microslice_buffer_size_];

#ifdef REBLOCK_PENN_USLICE
  size_t           remaining_size_;
  static const int remaining_buffer_size = 65536;
  uint8_t          remaining_ptr_[lbne::PennDataReceiver::remaining_buffer_size];

  lbne::PennMicroSlice::sample_count_t remaining_payloads_recvd_;
  lbne::PennMicroSlice::sample_count_t remaining_payloads_recvd_counter_;
  lbne::PennMicroSlice::sample_count_t remaining_payloads_recvd_trigger_;
  lbne::PennMicroSlice::sample_count_t remaining_payloads_recvd_timestamp_;
  lbne::PennMicroSlice::sample_count_t remaining_payloads_recvd_selftest_;
  lbne::PennMicroSlice::sample_count_t remaining_payloads_recvd_checksum_;

  uint64_t boundary_time_;    //unit is 64MHz NOvA clock ticks
  uint64_t run_start_time_;   //unit is 64MHz NOvA clock ticks

  uint64_t overlap_time_; //unit is 64MHz NOvA clock ticks
#endif //REBLOCK_PENN_USLICE
};


} /* namespace lbne */

#endif /* PENNDATARECEIVER_HH_ */
