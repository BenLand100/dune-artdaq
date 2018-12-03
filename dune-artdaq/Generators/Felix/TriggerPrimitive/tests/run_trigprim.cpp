#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <atomic>
#include <inttypes.h>
#include <unistd.h>
#include <array>
#include <cstdio>

#include "dune-artdaq/Generators/Felix/FelixFormat.hh"
#include "dune-artdaq/Generators/Felix/ProducerConsumerQueue.hh"

// #include "ReorderFacility.hh"
//#include "FelixReorder.hh"
// #include "FelixFormat.hh"
// #include "ReusableThread.hh"

#include "netio/netio.hpp"

#include <sys/resource.h> // For getrusage

#include "zmq.h"

#include "boost/program_options.hpp"

#include "../frame_expand.h"
#include "../process_avx2.h"
#include "../process_naive.h"
#include "../design_fir.h"
#include "save_hits.h"
#include "../ProcessingInfo.h"

namespace po = boost::program_options;

std::atomic<unsigned long long> messages_received;
std::atomic<unsigned long long> bytes_received;
std::atomic<unsigned int> m_size_err_cnt;

std::atomic<unsigned long long> sum_messages_received;

std::atomic<unsigned long long> windows_accepted;
std::atomic<unsigned long long> windows_rejected;


//======================================================================
struct Options
{
    Options(po::variables_map& vm)
        : host(vm["host"].as<std::string>()),
          port(vm["port"].as<unsigned short>()),
          backend(vm["backend"].as<std::string>()),
          tags(vm["tags"].as<std::vector<netio::tag>>()),
          pinCpu(vm["cpu"].as<unsigned>()),
          framesPerMsg(vm["framespermsg"].as<int>()),
          msgsize(sizeof(FelixFrame)*framesPerMsg),
          timeWindowNumMessages(vm["msgsperwindow"].as<int>()),
          timeWindowNumFrames(timeWindowNumMessages*framesPerMsg),
          byteSizeOut(timeWindowNumFrames*100*96),
          qsize(vm["qsize"].as<int>()),
          nthreads(vm["nthreads"].as<int>()),
          seconds(vm["seconds"].as<int>())
    {
        if(8%nthreads){
            std::cerr << "nthreads must evenly divide 8" << std::endl;
            exit(1);
        }
    }
    
    std::string host;
    unsigned short port;
    std::string backend;
    std::vector<netio::tag> tags;
    unsigned pinCpu;
    int framesPerMsg;
    size_t msgsize;
    size_t timeWindowNumMessages;
    size_t timeWindowNumFrames;
    size_t byteSizeOut;
    int qsize;
    int nthreads;
    int seconds;
};

//======================================================================
static void
run_subscribe(Options& opt, netio::context& ctx,
              size_t& badMsgCount,
              folly::ProducerConsumerQueue<MessageCollectionADCs>& m_pcq)
{

    // void *context = zmq_ctx_new();

    uint32_t msize = opt.msgsize;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    std::cout << " PIN to CPU core: " << opt.pinCpu << std::endl;
    CPU_SET(opt.pinCpu, &cpuset); // elinks go in jumps of 64 and we want the even CPUs

    // Pinning to CPU makes the performance behave in a confusing way,
    // possibly because all of the child threads get pinned in the
    // same way

    // pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);


    netio::subscribe_socket socket(&ctx, netio::sockcfg::cfg()(netio::sockcfg::ZERO_COPY));
    for(unsigned i=0; i<opt.tags.size(); i++)
        socket.subscribe(opt.tags[i], netio::endpoint(opt.host, opt.port));

    messages_received.store(0);

    // Thread pool idea copied from NetioHandler
    std::vector<std::thread> primfind_functors; 
    std::vector<uint16_t*> primfind_destinations;
    std::vector<std::atomic<int>> primfind_semaphores(opt.nthreads);
    for(auto& sem: primfind_semaphores){
        sem.store(-1);
    }
    std::vector<boost::shared_future<ProcessingInfo>> futures;

    std::atomic<bool> want_quit(false);
    MessageCollectionADCs* primfind_tmp=new MessageCollectionADCs[opt.nthreads*opt.timeWindowNumMessages];
    std::vector<size_t> threadWindowCounts(opt.nthreads, 0);

    const uint8_t registers_per_thread=REGISTERS_PER_FRAME/opt.nthreads;

    const int multiplier=1<<6;
    std::vector<int16_t> taps=firwin_int(7, 0.1, multiplier);
    taps.push_back(0); // Make it 8 long so it's a power of two

    for(int i=0; i<opt.nthreads; ++i){
        primfind_destinations.push_back(new uint16_t[opt.byteSizeOut]);

        ProcessingInfo pi(primfind_tmp,
                          opt.timeWindowNumFrames,
                          uint8_t(registers_per_thread*i),
                          uint8_t(registers_per_thread*(i+1)),
                          primfind_destinations[i],
                          taps.data(), (uint8_t)taps.size(),
                          multiplier,
                          0);

        futures.push_back(std::move(boost::make_ready_future<ProcessingInfo>(pi).share()));
    } // end for(i=0 to nThreads)

    sum_messages_received.store(1);

    typedef std::chrono::high_resolution_clock clock;
    auto t0 = clock::now();
    bool firstMessage=true;
    while( std::chrono::duration_cast<std::chrono::seconds>(clock::now()-t0).count() < opt.seconds )
    {
        netio::message m;
        socket.recv(m);

        if (m.size() == msize) {
            USR_CHAR_STRUCT ucs;
            m.serialize_to_usr_buffer((void*)&ucs);
            RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(ucs);
            MessageCollectionADCs* mca=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
            if(!m_pcq.write( std::move(*mca) )){
                std::cout << "Queue full" << std::endl;
            }
        } else {
            badMsgCount++;
        }
        if(sum_messages_received%opt.timeWindowNumMessages==0){
            // We've got enough frames to reorder them. First, copy
            // the data into a working area for the thread.  Given the
            // way expand_message_adcs orders the data, this results
            // in the relevant segment of primfind_tmp containing:
            //
            // (register 0, time  0) (register 0, time  1) ... (register 0, time 11)
            // (register 1, time  0) (register 1, time  1) ... (register 1, time 11)
            // ...
            // (register 7, time  0) (register 7, time  1) ... (register 7, time 11)
            // 
            // (register 0, time 12) (register 0, time 13) ... (register 0, time 23)
            // (register 1, time 12) (register 1, time 13) ... (register 1, time 23)
            // ...
            // (register 7, time 12) (register 7, time 13) ... (register 7, time 23)
            // 
            // ...
            // ...                                         ... (register 7, time timeWindowNumFrames-1)
            // Wait till all the currently running jobs are done...
            auto waiter=boost::when_all(futures.begin(), futures.end()).share();
            // ...and then copy the data into the working area
            auto copy_future=waiter.then(boost::launch::deferred,
                                         [&](decltype(waiter) f){
                                             // Zero out the output area
                                             // for(int i=0; i<opt.nthreads; ++i){
                                             //     memset(primfind_destinations[i], 0, sizeof(uint16_t)*opt.byteSizeOut);
                                             // }

                                             // Copy the window into the temporary working area
                                             for(size_t j = 0; j < opt.timeWindowNumMessages; j++){
                                                 m_pcq.read(primfind_tmp[j]);
                                             }
                                             std::vector<ProcessingInfo> pis;
                                             // If this is the first message we've processed,
                                             // set the pedestals, otherwise just copy
                                             // the ProcessingInfo unchanged from the input
                                             if(firstMessage){
                                                 // WindowCollectionADCs window(opt.timeWindowNumMessages, primfind_tmp);
                                                 for(int i=0; i<opt.nthreads; ++i){
                                                     // make a copy of the ProcessingInfo which we'll update
                                                     ProcessingInfo pi=f.get()[i].get();
                                                     pi.setState(primfind_tmp);
                                                     pis.push_back(pi);
                                                 }
                                                 firstMessage=false;
                                             }
                                             else{
                                                 for(int i=0; i<opt.nthreads; ++i){
                                                     pis.push_back(f.get()[i].get());
                                                 }
                                             }

                                             return pis;
                                         }).share();
            // ...and *then* set all the jobs going again. Each of the
            // new futures waits on the copy task, which is what we want.
            //
            // TODO: We're replacing the items in the vector of
            // futures. What happens to the old ones? Does the futures
            // system keep hold of them somehow?
            for(int ithread=0; ithread<opt.nthreads; ++ithread){
                futures[ithread]=copy_future.then(boost::launch::deferred,
                                                  [ithread](auto fin)
                                                  {
                                                      return process_window_avx2(fin.get()[ithread]);
                                                  });
            }
            // break; // Just go round the loop once, for testing
        } // end if(sum_msgs%128==0)
        messages_received++;
        sum_messages_received++;
        bytes_received += m.size();
    } // end while(true)

    std::cout << "get()ting all futures. Numbers of hits:" << std::endl;
    for(auto& f: futures){ std::cout << f.get().nhits << std::endl; }

    // for(int i=0; i<opt.nthreads; ++i){
    //     std::string filename(std::string("hits-thread")+std::to_string(i)+std::string(".txt"));
    //     save_avx2_hits(primfind_destinations[i], filename.c_str());
    // }

    std::cout << "Unsubscribing" << std::endl;
    for(unsigned i=0; i<opt.tags.size(); i++)
        socket.unsubscribe(opt.tags[i], netio::endpoint(opt.host, opt.port));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "run_subscribe ending" << std::endl;
}

//======================================================================
int
main(int argc, char** argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help",           "produce help message")
        ("host,h",         po::value<std::string>()->default_value("127.0.0.1"), "host to connect to")
        ("port,p",         po::value<unsigned short>()->default_value(12345), "port to connect to")
        ("backend,b",      po::value<std::string>()->default_value("posix"), "backend to connect to")
        ("tags,t",         po::value<std::vector<netio::tag>>()->default_value({1ul}, "1")->composing(), "tags to subscribe to")
        ("cpu,c",          po::value<unsigned>()->default_value(0), "CPU to pin to")
        ("framespermsg,f", po::value<int>()->default_value(FRAMES_PER_MSG), "number of WIB frames in each netio message")
        ("msgsperwindow",  po::value<int>()->default_value(128), "messages per reordering window")
        ("qsize,q",        po::value<int>()->default_value(20000), "number of items in queue")
        ("nthreads",       po::value<int>()->default_value(8), "number of reordering threads")
        ("seconds,s",      po::value<int>()->default_value(10), "seconds to run for")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if(vm.count("help") || vm.empty()) {
        std::cout << desc << "\n";
        return 1;
    }

    if(!vm.count("tags")){
        std::cout << "No tags specified" << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    Options opt(vm);

    netio::context ctx(opt.backend.c_str());

    typedef std::chrono::high_resolution_clock clock;
    auto t0 = clock::now();

    folly::ProducerConsumerQueue<MessageCollectionADCs> m_pcq(opt.qsize);

    size_t m_badMsgs = 0;

    double prev_sys=0;
    double prev_user=0;
    netio::timer tmr(ctx.event_loop(), [&](void*){
            auto t1 = clock::now();
            double seconds =  std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count()/1000000.;
            unsigned long long messages_delta = std::atomic_exchange(&messages_received, 0ull);
            unsigned long long bytes_delta = std::atomic_exchange(&bytes_received, 0ull);
            unsigned long long windows_acc_delta = std::atomic_exchange(&windows_accepted, 0ull);
            unsigned long long windows_rej_delta = std::atomic_exchange(&windows_rejected, 0ull);

            std::cout << "Statistics - timestamp " << std::chrono::duration_cast<std::chrono::seconds>(t1.time_since_epoch()).count() << std::endl;
            std::cout << "  timedelta [s]: " << seconds << std::endl;
            std::cout << "  data received [MB]: " << bytes_delta/1024./1024. << std::endl;
            std::cout << "  messages_received: " << messages_delta << std::endl;
            std::cout << "  message rate [kHz]: " << messages_delta/seconds/1000 << std::endl;
            std::cout << "  data rate [GB/s]: " << bytes_delta/seconds/1024./1024./1024. << std::endl;
            std::cout << "  data rate [Gb/s]: " << bytes_delta/seconds/1024./1024./1024.*8 << std::endl;
            std::cout << "  sum badMsgs: " << m_badMsgs << std::endl;
            std::cout << "  qSize: " << m_pcq.sizeGuess() << std::endl;
            std::cout << "  windows accepted: " << windows_acc_delta << std::endl;
            std::cout << "  windows rejected: " << windows_rej_delta << std::endl;
            if(messages_delta > 0)
                std::cout << "  average message size [Byte]: " << bytes_delta/messages_delta << std::endl;
            std::cout << "  msg size errors: " << m_size_err_cnt << std::endl;
            t0 = t1;
            struct rusage ru;
            getrusage(RUSAGE_SELF, &ru);
            double userCpu = (double)(ru.ru_utime.tv_sec) +
                ((double)(ru.ru_utime.tv_usec) / 1000000.);
            double sysCpu = (double)(ru.ru_stime.tv_sec) +
                ((double)(ru.ru_stime.tv_usec) / 1000000.);
            printf("  cpu used (user, sys, user %%) [s]: %.2f  %.2f  %.0f\n", (userCpu-prev_user), (sysCpu-prev_sys), 100*((userCpu-prev_user)/seconds));
            prev_sys=sysCpu;
            prev_user=userCpu;
        }, NULL);

    tmr.start(2000);

    std::thread bg_thread([&ctx](){
            ctx.event_loop()->run_forever();
        });

    run_subscribe(opt, ctx, m_badMsgs, m_pcq);

    std::cout << "Stopping ctx" << std::endl;
    ctx.event_loop()->stop();
    std::cout << "Waiting for BG thread" << std::endl;
    bg_thread.join();

    std::cout << "done" << std::endl;
}
