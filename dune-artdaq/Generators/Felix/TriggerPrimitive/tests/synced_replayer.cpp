#include "ptmp/api.h"
#include "czmq.h"

#include "CLI11.hpp"

#include "dune-artdaq/Generators/swTrigger/ptmp_util.hh"

#include <cstdio>
#include <thread>
#include <chrono>

// Every message data is assumed to be prefixed by a 64 bit unsigned
// int holding the size of the message data in bytes.
zmsg_t* read_msg(FILE* fp)
{
    size_t size=0;
    int nread = fread(&size, sizeof(size_t), 1, fp);
    if (nread != 1) {
        return NULL;
    }
    zchunk_t* chunk = zchunk_read(fp, size);
    zframe_t* frame = zchunk_pack(chunk);
    zchunk_destroy(&chunk);
    zmsg_t* msg = zmsg_decode(frame);
    zframe_destroy(&frame);
    return msg;
}

class LinkInfo
{
public:
    LinkInfo(std::string filename, std::string replayer_out, int index)
        : m_index(index),
          m_filename(filename),
          m_fp(fopen(m_filename.c_str(), "r")),
          m_raw_sock(zsock_new(ZMQ_PUSH)),
          raw_sock_endpoint(std::string("inproc://sets")+std::to_string(index)),
          m_receiver(ptmp_util::make_ptmp_socket_string("PULL", "connect", {raw_sock_endpoint})),
          to_replayer_endpoint(std::string("tcp://10.73.136.67:1614")+std::to_string(index)),
          m_sender(ptmp_util::make_ptmp_socket_string("PUB", "bind", {to_replayer_endpoint})),
          m_replayer(ptmp_util::make_ptmp_tpwindow_string({to_replayer_endpoint},
                                                          {replayer_out},
                                                          1, 1,
                                                          "SUB",
                                                          "PUSH")),
        m_tpset(new ptmp::data::TPSet)


    {
        std::cout << "Constructed LinkInfo " << m_index << ". filename " << m_filename << std::endl;
        zsock_bind(m_raw_sock, raw_sock_endpoint.c_str());
        read_one_msg();
    }

    ~LinkInfo()
    {
        zsock_destroy(&m_raw_sock);
        fclose(m_fp);
        delete m_tpset;
    }

    bool read_one_msg()
    {
        zmsg_t* msg = read_msg(m_fp);
        if(!msg){
            m_tpset=nullptr;
            return false;
        }
        zsock_send(m_raw_sock, "m", msg);
        m_receiver(*m_tpset);
        return true;
    }

    ptmp::data::data_time_t next_tpset_tstart()
    {
        if(!m_tpset) return 0;
        return m_tpset->tstart();
    }

    // Send the current TPSet out and read the next one. Return value
    // is whether there is another TPSet
    bool send_tpset()
    {
        std::cout << "Sending TPSet with tstart 0x" << std::hex << m_tpset->tstart() << " from link " << std::dec  << m_index << std::endl;
        m_sender(*m_tpset);
        return read_one_msg();
    }

    // Discard the current TPSet without sending it, and read the next
    // one. Return value is whether there is another TPSet
    bool discard_tpset()
    {
        m_tpset->clear();
        return read_one_msg();
    }

    // Read and discard messages until a msg with tstart>t is
    // found. Return value is whether there is another TPSet
    bool discard_until(ptmp::data::data_time_t t)
    {
        while(m_tpset->tstart()<=t){
            if(!discard_tpset()) return false;
        }
        return true;
    }

    int m_index;

    std::string m_filename;
    // File of raw zmsg_t messages
    FILE* m_fp;
    // Raw czmq socket that sends the raw zmsg_t messages...
    zsock_t* m_raw_sock;
    std::string raw_sock_endpoint;
    // ...receives the raw messages, ie converting them to TPSets
    ptmp::TPReceiver m_receiver;
    std::string to_replayer_endpoint;
    // ...sends the TPSets on to the replayer, when ready
    ptmp::TPSender m_sender;

    // Replayer
    ptmp::TPReplay m_replayer;

    ptmp::data::TPSet* m_tpset;

};


int main(int argc, char** argv)
{
    CLI::App app{"Send multiple streams from disk to TPReplay, trying to keep them in sync"};

    std::string config_file;
    app.add_option("-c", config_file, "Config file", false);

    int nsets=1000;
    app.add_option("-n", nsets, "number of TPSets to print (-1 for no limit)", true);

    CLI11_PARSE(app, argc, argv);

    std::ifstream conf_in(config_file.c_str());
    std::string filename, replay_socket;
    std::vector<std::unique_ptr<LinkInfo>> links;
    int link_counter=0;
    while(conf_in >> filename >> replay_socket){
        links.emplace_back(std::make_unique<LinkInfo>(filename, replay_socket, link_counter++));
    }

    // Sleep a bit so everything can set up
    std::cout << "Sleeping..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // First, find the file with the latest start time, and
    // "fast-forward" all the others to that time, so we're all in
    // sync
    ptmp::data::data_time_t max_firstTime=0;
    for(auto& link: links){
        ptmp::data::data_time_t tstart=link->next_tpset_tstart();
        if(tstart>max_tstart){
            max_tstart=tstart;
        }
    }
    bool all_ok=true;
    for(auto& link: links){
        link.discard_until(max_tstart);
    }

    int set_counter=0;
    while(true){
        ptmp::data::data_time_t min_tstart=UINT64_MAX;
        LinkInfo* min_link=nullptr;
        for(auto& link: links){
            ptmp::data::data_time_t tstart=link->next_tpset_tstart();
            if(tstart<min_tstart){
                min_tstart=tstart;
                min_link=link.get();
            }
        }
        // Send the earliest time message
        if(!min_link->send_tpset()) break;
        if(nsets>0 && set_counter++>nsets) break;
    }

    links.clear();
}
