#include "traccc/cuda/utils/Sync.hpp"

#include <iostream>
namespace Sync{
    
void init_shared_mem(int n_proc, int u_id,int region_size,unsigned char* mem){
    Sync::n_proc=n_proc;
    Sync::u_id=u_id;
    Sync::region_size=region_size;
    //boost::interprocess::interprocess_mutex mutex = pmutex;

    //unsigned char *mem = static_cast<unsigned char*>(region.get_address());
    unsigned char zero = 0x00;
    std::memset(mem+u_id, zero, sizeof (zero));
}


void reset_shared_mem(unsigned char* mem){
    
    //boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);
    mutex.lock();
    printf("reseting\n");
    //unsigned char zero = 0x00;
    //std::memset(mem+Sync::u_id, zero, sizeof (zero));
    mem[u_id]=0x00;
    
    mutex.unlock();

}




void complete(unsigned char* mem){
    
    //boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);
    mutex.lock();
    printf("before changing shared mem\n"); 

    //std::memset(region.get_address()+1, 1, sizeof(char));
    //unsigned char *mem = static_cast<unsigned char*>(region.get_address());
    
    mem[u_id]=0x01;

    for(std::size_t i = 0; i < region_size; ++i){
        printf("index %d %d\n",i,*mem);
        mem++;
    }
    mutex.unlock();
}


void wait_for_other_processes(unsigned char* mem){

    
    int c = 0;
    while (c<n_proc){
        unsigned char* temp_mem = mem;
        c=0;
        sleep(0.0001);
        //unsigned char *mem = static_cast<unsigned char*>(region.get_address());
        for(std::size_t i = 0; i < region_size; ++i){
            if(*temp_mem == 1)   
                c++ ;
            temp_mem++;
        }
        //printf("c %d and n_proc %d\n",c,n_proc);
            
    }
    //printf("This should be printed at once? 5 times \n");
}


};

