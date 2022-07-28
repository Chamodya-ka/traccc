#pragma once
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
using namespace boost::interprocess;

class Sync{
    public:
        Sync (int n_proc, int u_id);
        void wait_for_other_processes();
        void delete_shared_mem();
        void complete(int u_id);
    private:
        shared_memory_object shm_obj;
        mapped_region region;
        interprocess_mutex mutex;
        int n_proc;
};
