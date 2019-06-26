#include "numa.h"
#include "numaif.h"
#include "errno.h"
#include "sched.h" // For sched_getcpu
#include "unistd.h" // For usleep

#include <iostream>
#include <thread>
#include <mutex>


std::mutex gMutex;

std::string current_mempolicy()
{
    int policy=0;
    int gmp_ret=get_mempolicy(&policy, NULL, 0, 0, 0);
    if(gmp_ret!=0){
        std::cout << "get_mempolicy returned " << gmp_ret << ". errno=" << errno << std::endl;
        perror(0);
        return "Error";
    }
    std::string ret{"Unknown"};
    switch(policy){
    case MPOL_DEFAULT:
        ret="Default";
        break;
    case MPOL_PREFERRED:
        ret="Preferred";
        break;
    case MPOL_BIND:
        ret="Bind";
        break;
    case MPOL_INTERLEAVE:
        ret="Interleave";
        break;
    }
    return ret;

}

int which_node(void *p)
{
    // long pagesize=numa_pagesize();
    // void *p_page=(void*)((long)p & ~(pagesize-1));
    // int move_pages_node = -1;
    // long ret=move_pages(0, 1, &p_page, NULL, &move_pages_node, MPOL_MF_MOVE);
    // if(ret==-1) std::cerr << "move_pages gave error" << std::endl;

    int get_mempolicy_node = -1;
    int ret=get_mempolicy(&get_mempolicy_node, NULL, 0, p, MPOL_F_NODE | MPOL_F_ADDR);
    if(ret==-1) std::cerr << "get_mempolicy gave error" << std::endl;
    // if(get_mempolicy_node!=move_pages_node){
    //     std::cerr << "move_pages said " << move_pages_node << " but get_mempolicy said " << get_mempolicy_node << std::endl;
    // }
    return get_mempolicy_node;
}

void pin_and_allocate(int cpu, bool set_local)
{
    std::lock_guard<std::mutex> lock(gMutex);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    int pin_ret=pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if(pin_ret!=0){
        std::cerr << "setaffinity failed" << std::endl;
    }
    if(set_local) numa_set_localalloc();

    std::string policy=current_mempolicy();
    // Allocate a few pages of data and access them to make sure they're faulted in
    const size_t N=10*numa_pagesize();
    // char* a=(char*)numa_alloc_onnode(N*sizeof(char), numa_node_of_cpu(cpu));
    char* a=new char[N];
    for(size_t i=0; i<N; ++i) a[i]=3;
    int a_node=which_node(a+numa_pagesize());
    // numa_free(a, N*sizeof(char));
        
    std::cout << "In thread pinned to CPU " << cpu << " (node " << numa_node_of_cpu(cpu) << "), actually on cpu " << sched_getcpu() << ", with set_local=" << set_local << ", mem policy is " << policy << ", pointer a is allocated on node " << a_node << std::endl;
    std::cout << std::endl;
}

int main(int argc, char** argv)
{
    int available=numa_available();
    std::cout << "numa_available? " << available << std::endl;
    if(available==-1){
        std::cerr << "numa not available" << std::endl;
        exit(1);
    }

    int cpu=0;
    if(argc>1) cpu=atoi(argv[1]);
    std::cout << "Pinning to CPU " << cpu << " which is on node " << numa_node_of_cpu(cpu) << std::endl;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    int pin_ret=pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if(pin_ret!=0){
        std::cerr << "settaffinity failed" << std::endl;
    }

    std::cout << "Before set_localalloc, policy is " << current_mempolicy() << std::endl;
    numa_set_localalloc();
    std::cout << "After set_localalloc, policy is " << current_mempolicy() << std::endl;

    int max_node=numa_max_node();
    std::cout << "The highest numa node is " << max_node << std::endl;

    int num_nodes=numa_num_task_nodes();
    std::cout << "The number of allowed nodes is " << num_nodes << std::endl;

    int preferred=numa_preferred();
    std::cout << "The preferred numa node is " << preferred << std::endl;

    bitmask* membind=numa_get_membind();
    for(unsigned int i=0; i<=(unsigned int)max_node; ++i) std::cout << "membind " << i << "?: " << numa_bitmask_isbitset(membind, i) << std::endl;

    long pagesize=numa_pagesize();
    std::cout << "The page size is " << pagesize << " bytes" << std::endl;

    // ---------------------------------------------------------
    // Allocate a few pages of data and access them to make sure they're faulted in
    const size_t N=10*numa_pagesize();
    char* a=(char*)numa_alloc_local(N*sizeof(char));
    for(size_t i=0; i<N; ++i) a[i]=3;
    int a_node=which_node(a+numa_pagesize());
    numa_free(a, N*sizeof(char));

    std::cout << "In main thread, pinned to CPU " << cpu << " (node " << numa_node_of_cpu(cpu) << "), pointer a is allocated on node " << a_node << std::endl;
    // ---------------------------------------------------------
    // const size_t N=10*numa_pagesize();
    char* node0=(char*)numa_alloc_onnode(N*sizeof(char), 0);
    for(size_t i=0; i<N; ++i) node0[i]=3;
    int node0_node=which_node(node0+numa_pagesize());
    numa_free(node0, N*sizeof(char));
    std::cout << "Memory allocated with numa_alloc_onnode(..., 0) is actually on node " << node0_node << std::endl;

    // ---------------------------------------------------------
    // const size_t N=10*numa_pagesize();
    char* node1=(char*)numa_alloc_onnode(N*sizeof(char), 1);
    for(size_t i=0; i<N; ++i) node1[i]=3;
    int node1_node=which_node(node1+numa_pagesize());
    numa_free(node1, N*sizeof(char));
    std::cout << "Memory allocated with numa_alloc_onnode(..., 1) is actually on node " << node1_node << std::endl;

    {
        std::thread athr=std::thread(pin_and_allocate, 0, false);
        athr.join();
        std::thread bthr=std::thread(pin_and_allocate, 1, false);
        bthr.join();
        std::thread cthr=std::thread(pin_and_allocate, 18, false);
        cthr.join();
        std::thread dthr=std::thread(pin_and_allocate, 19, false);
        dthr.join();
    }

    {
        std::thread athr=std::thread(pin_and_allocate, 0, true);
        athr.join();
        std::thread bthr=std::thread(pin_and_allocate, 1, true);
        bthr.join();
        std::thread cthr=std::thread(pin_and_allocate, 18, true);
        cthr.join();
        std::thread dthr=std::thread(pin_and_allocate, 19, true);
        dthr.join();
    }

}
