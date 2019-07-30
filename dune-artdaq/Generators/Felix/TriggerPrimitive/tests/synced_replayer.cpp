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
    // Beware replayer_pattern=PUSH! Push sockets block on send if no
    // one is listening. TPReplay detects this case and exits, so we
    // can only do anything useful on a TPReplay with PUSH output if
    // there's a sink downstream
    LinkInfo(std::string filename, std::string replayer_out, std::string replayer_pattern, int index)
        : m_index(index),
          m_filename(filename),
          m_fp(fopen(m_filename.c_str(), "r")),
          m_raw_sock(zsock_new(ZMQ_PUSH)),
          raw_sock_endpoint(std::string("inproc://sets")+std::to_string(index)),
          m_receiver(ptmp_util::make_ptmp_socket_string("PULL", "connect", {raw_sock_endpoint})),
          to_replayer_endpoint(std::string("inproc://replay")+std::to_string(index)),
          m_replayer(ptmp_util::make_ptmp_tpwindow_string({to_replayer_endpoint},
                                                          {replayer_out},
                                                          1, 1,
                                                          "PULL",
                                                          replayer_pattern)),
          m_sender(ptmp_util::make_ptmp_socket_string("PUSH", "bind", {to_replayer_endpoint})),
          m_tpset(new ptmp::data::TPSet),
          m_n_sent(0)
    {
        if(!m_fp){
            std::cerr << "Couldn't open file " << m_filename << std::endl;
            exit(1);
        }
        std::cout << "Constructed LinkInfo " << m_index << ". filename " << m_filename << std::endl;
        zsock_bind(m_raw_sock, raw_sock_endpoint.c_str());
        read_one_msg();
    }

    LinkInfo(LinkInfo&) = delete;

    ~LinkInfo()
    {
        zsys_info("Closing link %d after sending %ld", m_index, m_n_sent);
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
        if(m_n_sent<100){
            std::cout << "Link " << m_index << "sending count=" << m_tpset->count() << " detid=0x" << std::hex << m_tpset->detid() << " tstart 0x" << std::hex << m_tpset->tstart() << std::dec << std::endl;
        }
        m_sender(*m_tpset);
        ++m_n_sent;
        return read_one_msg();
    }

    // Discard the current TPSet without sending it, and read the next
    // one. Return value is whether there is another TPSet
    bool discard_tpset()
    {
        m_tpset->Clear();
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
    // Replayer
    ptmp::TPReplay m_replayer;
    // ...sends the TPSets on to the replayer, when ready
    ptmp::TPSender m_sender;
    ptmp::data::TPSet* m_tpset;
    size_t m_n_sent;
};


int main(int argc, char** argv)
{
    CLI::App app{"Send multiple streams from disk to TPReplay, trying to keep them in sync"};

    std::string config_file;
    app.add_option("-c", config_file, "Config file", false);

    int nsets=-1;
    app.add_option("-n", nsets, "number of TPSets to print (-1 for no limit)", true);

    CLI11_PARSE(app, argc, argv);

    std::ifstream conf_in(config_file.c_str());
    std::string filename, replay_socket, replay_pattern;
    std::vector<std::unique_ptr<LinkInfo>> links;
    int link_counter=0;
    while(conf_in >> filename >> replay_socket >> replay_pattern){
        links.emplace_back(std::make_unique<LinkInfo>(filename, replay_socket, replay_pattern, link_counter++));
    }

    // Sleep a bit so everything can set up
    std::cout << "Sleeping..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
        // First, find the file with the latest start time, and
        // "fast-forward" all the others to that time, so we're all in
        // sync
        ptmp::data::data_time_t max_tstart=0;
        for(auto& link: links){
            ptmp::data::data_time_t tstart=link->next_tpset_tstart();
            if(tstart>max_tstart){
                max_tstart=tstart;
            }
        }
        for(auto& link: links){
            link->discard_until(max_tstart);
        }
    }

    int set_counter=0;
    while(true){
        ptmp::data::data_time_t min_tstart=UINT64_MAX;
        LinkInfo* min_link=nullptr;
        int min_link_index=0;
        for(auto& link: links){
            ptmp::data::data_time_t tstart=link->next_tpset_tstart();
            if(tstart<min_tstart){
                min_tstart=tstart;
                min_link=link.get();
                min_link_index=link->m_index;
            }
        }
        // Send the earliest time message
        if(!min_link->send_tpset()){
            zsys_info("Link %d finished. Exiting", min_link_index);
            break;
        }
        if(nsets>0 && set_counter++>=nsets){
            zsys_info("Sent %d sets as requested. Exiting", nsets);
            break;
        }
    }

    // Sleep a bit so the replay threads can catch up
    std::cout << "Sleeping..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    links.clear();
    return 0;
}
