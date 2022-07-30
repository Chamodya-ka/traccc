#pragma once
//#include <boost/interprocess/shared_memory_object.hpp>
//#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
using namespace boost::interprocess;


namespace Sync{
    void init_shared_mem(int n_proc, int u_id,unsigned char* mem);
    void wait_for_other_processes(unsigned char* mem,size_t region_size);
    void complete(int u_id,unsigned char* mem,size_t region_size);
    void reset_shared_mem(int u_id,unsigned char* mem);
    //static shared_memory_object shm_obj;
    //static mapped_region region;
    interprocess_mutex mutex;
    int n_proc;
}




