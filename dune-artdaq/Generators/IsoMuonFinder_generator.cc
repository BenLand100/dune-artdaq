//################################################################################
//# /*
//#        IsoMuonFinder_generator.cc
//#        Borrowed heavily from SWTrigger_generator.cc and HitFinderCPUReceiver_generator.cc
//#
//#  JSensenig
//#  June 2019
//#  for ProtoDUNE
//# */
//###############################################################################

#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_IsoMuonFinder").c_str()
#define TLVL_ISMUON 10

#include "dune-artdaq/Generators/IsoMuonFinder.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/swTrigger/ptmp_util.hh"
#include "dune-artdaq/Generators/swTrigger/trigger_util.hh"

#include "artdaq/Generators/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/TimingFragment.hh"

#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <set>

#include <unistd.h>

#include <cstdio>

#include "artdaq/Application/BoardReaderCore.hh"

#include "timingBoard/FragmentPublisher.hh"

using namespace dune;

// Constructor ------------------------------------------------------------------------------
dune::IsoMuonFinder::IsoMuonFinder(fhicl::ParameterSet const & ps):
    CommandableFragmentGenerator(ps),
    instance_name_("IsoMuonFinder"),
    timeout_(ps.get<int>("timeout")), 
    stopping_flag_(0),
    fragment_publisher_(new artdaq::FragmentPublisher(ps.get<std::string>("zmq_fragment_connection_out"))),
    tpwinsocks_(ptmp_util::endpoints_for_key(ps, "tpwindow_input_connections_key")), 
    tpwoutsocks_(ptmp_util::endpoints_for_key(ps, "tpwindow_output_connections_key")), 
    tspan_(ps.get<uint64_t>("ptmp_tspan")),
    tbuf_(ps.get<uint64_t>("ptmp_tbuffer")),
    tpzipinsocks_(tpwoutsocks_), 
    tpzipout_(ps.get<std::string>("tpsorted_output")), 
    tardy_(ps.get<int>("ptmp_tardy")),
    hit_per_link_threshold_(ps.get<size_t>("hit_per_link_threshold")),
    trigger_holdoff_time_(ps.get<uint64_t>("trigger_holdoff_time_pdts_ticks"))
{
    DAQLogger::LogInfo(instance_name_) << "Initiated IsoMuonFinder BoardReader\n";
    if(tpwinsocks_.size()!=tpwoutsocks_.size()){
        throw cet::exception("Size of TPWindow input and output socket configs do not match. Check tpwindow_inputs and tpwindow_outputs fhicl parameters");
    }


    fragment_publisher_->BindPublisher();

}

dune::IsoMuonFinder::~IsoMuonFinder()
{
    DAQLogger::LogInfo(instance_name_) << "dtor";
}

// start() routine --------------------------------------------------------------------------
void dune::IsoMuonFinder::start(void)
{
    stopping_flag_.store(false);

    DAQLogger::LogInfo(instance_name_) << "TPWindow Tspan " << tspan_ << " and Tbuffer " << tbuf_;

    if(tpwinsocks_.size()!=tpwoutsocks_.size()){
        DAQLogger::LogError(instance_name_) << "TPWindow input and output lists are different lengths!";
    }
    for(size_t i=0; i<tpwinsocks_.size(); ++i){
        DAQLogger::LogInfo(instance_name_) << "Creating TPWindow from " << tpwinsocks_.at(i) << " to " << tpwoutsocks_.at(i);
        std::string jsonconfig{ptmp_util::make_ptmp_tpwindow_string({tpwinsocks_.at(i)},
                                                                    {tpwoutsocks_.at(i)},
                                                                    tspan_,tbuf_,
                                                                    "SUB",
                                                                    "PUSH")};
        tpwindows_.push_back( std::make_unique<ptmp::TPWindow>(jsonconfig) );
    }

    DAQLogger::LogInfo(instance_name_) << "TPZipper tardy is set to " << tardy_;

    // TPSorted connection: TPWindow --> TPSorted --> IsoMuonFinder BR (default policy is drop)
    tpzipper_.reset(new ptmp::TPZipper( ptmp_util::make_ptmp_tpsorted_string(tpzipinsocks_,{tpzipout_},tardy_,"PULL", "PUSH") ));

    // Start a TPSet receiving/sending thread
    tpset_handler = std::thread(&dune::IsoMuonFinder::tpsetHandler,this);
}

// tpsetHandler() routine ------------------------------------------------------------------
void dune::IsoMuonFinder::tpsetHandler() {

    pthread_setname_np(pthread_self(), "tpsethandler");

    DAQLogger::LogInfo(instance_name_) << "Starting TPSet handler thread.";

    ptmp::TPReceiver* receiver=new ptmp::TPReceiver( ptmp_util::make_ptmp_socket_string("PULL","connect",{tpzipout_}) );

    // Debugging stats
    size_t n_n_sources[10]={0};
    size_t n_sets_total=0;
    size_t n_sets_wall=0;
    size_t n_sets_tpc=0;
    size_t max_n_chan_hit=0;
    size_t n_triggers=0;

    uint64_t last_tstart=0;
    size_t n_sources=0;
    size_t max_n_sources=0;

    const size_t apa5_offline_number=1;
    // const size_t apa6_offline_number=3;
    const size_t channels_per_apa=2560;
    const size_t min_channel=channels_per_apa*apa5_offline_number;
    // Did we get a hit on this channel in this time window? Extremely
    // dumb because it has a spot for every possible channel in APA 5
    // and 6, even though most are induction and won't have hits. But
    // this makes the later code easier
    uint8_t has_hit[2*channels_per_apa]={0,};
    while(!stopping_flag_.load()) { 
        ptmp::data::TPSet in_set;
        bool received = (*receiver)(in_set, timeout_);
        if(received){
            ++n_sets_total;
            // If the data is from a wall-facing link, just ignore
            // it. In APA 5, it turns out that the wall-facing links
            // are all fiber 2. detid contains (fiber_no << 16) |
            // (slot_no << 8) | m_crate_no
            size_t fiber_no=(in_set.detid() >> 16) & 0xff;
            if(fiber_no==2){
                ++n_sets_wall;
                continue;
            }
            ++n_sets_tpc;
            if(in_set.tstart() >= last_tstart+tspan_){
                // This is the first item from a new time
                // window. Decide whether the previous one should
                // trigger, based on n_sources and how many items in
                // has_hit are set, then reset last_tstart, clear has_hit and set n_sources=0
                max_n_sources=std::max(n_sources, max_n_sources);
                if(n_sources<10) n_n_sources[n_sources]++;
                size_t n_chan_hit=0;
                for(size_t i=0; i<2*channels_per_apa; ++i) n_chan_hit+=has_hit[i];
                max_n_chan_hit=std::max(max_n_chan_hit, n_chan_hit);
                if(n_chan_hit>=hit_per_link_threshold_*max_n_sources){
                    // Trigger!
                    DAQLogger::LogInfo(instance_name_) << "Requesting trigger at 0x" << std::hex << last_tstart << std::dec << " with " << n_chan_hit
                                                       << " (threshold " << (hit_per_link_threshold_*max_n_sources) << ")";
                    timestamp_queue_.write(last_tstart);
                    ++n_triggers;
                }
                last_tstart=in_set.tstart();
                memset(has_hit, 0, 2*channels_per_apa*sizeof(uint8_t));
                n_sources=0;
            }
            ++n_sources;

            for(auto const& tp: in_set.tps()) has_hit[tp.channel()-min_channel]=1;
        }

    }

    DAQLogger::LogInfo(instance_name_) << "Received " << n_sets_total << " TPSets. TPC-facing: " << n_sets_tpc << ", Wall-facing: " << n_sets_wall;
    DAQLogger::LogInfo(instance_name_) << "max_n_chan_hit: " << max_n_chan_hit;
    std::stringstream ss;
    for(size_t i=0; i<10; ++i) ss << n_n_sources[i] << " ";
    DAQLogger::LogInfo(instance_name_) << "n_n_sources[]: " << ss.str();
    DAQLogger::LogInfo(instance_name_) << "Issued " << n_triggers << " triggers";
    delete receiver;
}
 
// stop() routine --------------------------------------------------------------------------
void dune::IsoMuonFinder::stop(void)
{
    DAQLogger::LogInfo(instance_name_) << "stop() called";
    stopping_flag_.store(true);

    DAQLogger::LogInfo(instance_name_) << "Joining tpset_handler thread...";
    tpset_handler.join();
    DAQLogger::LogInfo(instance_name_) << "tpset_handler thread joined";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Destruct all the TPWindow instances and the TPZipper
    DAQLogger::LogInfo(instance_name_) << "Destructing the PTMP sockets in thread " << pthread_self();
    tpzipper_.reset(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    tpwindows_.clear(); 

    DAQLogger::LogInfo(instance_name_) << "Destroyed PTMP windowing and sorting threads.";
}


//-----------------------------------------------------------------------
void dune::IsoMuonFinder::stopNoMutex(void)
{
    DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";
}

//-----------------------------------------------------------------------
std::unique_ptr<artdaq::Fragment> IsoMuonFinder::makeFragment(uint64_t timestamp)
{
    return trigger_util::makeTriggeringFragment(timestamp, ev_counter(), fragment_id());
}

//--------------------------------------------------------------------------
bool dune::IsoMuonFinder::getNext_(artdaq::FragmentPtrs& frags)
{
    static uint64_t prev_timestamp=0;

    // TODO: Change the check here to "if stopping_flag && we've got all of the fragments in the queue"
    if (stopping_flag_.load()) return false;

    uint64_t* trigger_timestamp = timestamp_queue_.frontPtr();
    if (trigger_timestamp) {
        DAQLogger::LogInfo(instance_name_) << "Trigger requested for 0x" << std::hex << (*trigger_timestamp) << std::dec;
        if(*trigger_timestamp < prev_timestamp+trigger_holdoff_time_){
            DAQLogger::LogInfo(instance_name_) << "Trigger too close to previous trigger time of " << prev_timestamp << ". Not sending";
        }
        else{
            std::unique_ptr<artdaq::Fragment> frag=makeFragment(*trigger_timestamp);
            dune::TimingFragment timingFrag(*frag);    // Overlay class
            
            int pubSuccess = fragment_publisher_->PublishFragment(frag.get(), &timingFrag);
            if (!pubSuccess)
                DAQLogger::LogInfo(instance_name_) << "Publishing fragment to ZeroMQ failed";
            
            frags.emplace_back(std::move(frag));
            // We only increment the event counter for events we send out
            ev_counter_inc();
            
            prev_timestamp=*trigger_timestamp;
        }
        timestamp_queue_.popFront();
    }
    // Sleep a bit so we don't spin the CPU
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return true;

}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::IsoMuonFinder)
