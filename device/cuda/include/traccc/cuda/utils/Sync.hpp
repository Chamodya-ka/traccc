#pragma once

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
namespace Sync{
    void init_shared_mem(int n_proc, int u_id, int region_size, unsigned char* mem);
    void wait_for_other_processes(unsigned char* mem);
    void complete(unsigned char* mem);
    void reset_shared_mem(unsigned char* mem);
    void wait_for_reset(unsigned char* mem);
    int get_uid();
    
    static int n_proc;
    static int u_id;
    static int region_size;

}




