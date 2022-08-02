#pragma once
//#include <boost/interprocess/sync/interprocess_mutex.hpp>
//#include <boost/interprocess/sync/scoped_lock.hpp>
//#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
namespace Sync{
    void init_shared_mem(int n_proc, int u_id, int region_size, unsigned char* mem);
    void wait_for_other_processes(unsigned char* mem);
    void complete(unsigned char* mem);
    void reset_shared_mem(unsigned char* mem);
    void wait_for_reset(unsigned char* mem);
    //static shared_memory_object shm_obj;
    //static mapped_region region;
    //static boost::interprocess::interprocess_mutex mutex;
    static int n_proc;
    static int u_id;
    static int region_size;
    //static bool done_reset;
    //static bool done_set;
    /* static struct mutex_remove
      {
         mutex_remove() { boost::interprocess::named_mutex::remove("named_mutex"); }
         ~mutex_remove(){ boost::interprocess::named_mutex::remove("named_mutex"); }
      } remover;
    boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create, "named_mutex"); */
    //boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create, "named_mutex");
}




