#include "traccc/cuda/utils/Sync.hpp"
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <iostream>

using namespace boost::interprocess;


Sync::Sync (int n_proc,int u_id): shm_obj(open_or_create,"shared_memory",read_write), region(shm_obj,read_write){
    this->n_proc=n_proc;
    struct shm_remove
    {
        shm_remove() { shared_memory_object::remove("MySharedMemory"); }
        ~shm_remove(){ shared_memory_object::remove("MySharedMemory"); }
    } remover;

    
    shm_obj.truncate(n_proc);
    
    //printf("sharemem of %d initialized \n",n_proc);
    /* 
    try{
        shm_obj
            (open_or_create                  
            ,"shared_memory"              //name
            ,read_write                   //read-write mode
            );
            
    }            
    catch (const std::exception& ex){
        std::cout<<"exception "<<std::endl;
    } 
    */
    // Set position of uid to 0 
    unsigned char *mem = static_cast<unsigned char*>(region.get_address());
    unsigned char zero = 0x00;
    std::memset(mem+u_id, zero, sizeof (zero));
    //printf("initialized region of size %d \n",region.get_size());
}

void Sync::delete_shared_mem(){
    shared_memory_object::remove("shared_memory");
}



void Sync::complete(int u_id){
    printf("before changing shared mem\n");
    
    //std::memset(region.get_address()+1, 1, sizeof(char));
    unsigned char *mem = static_cast<unsigned char*>(region.get_address());
    mutex.lock();
    mem[u_id]=0x01;
    mutex.unlock();
    for(std::size_t i = 0; i < region.get_size(); ++i){
        //printf("index %d %d\n",i,*mem);
        mem++;
    }
}

void Sync::wait_for_other_processes(){
    int c = 0;
    while (c<n_proc){
        c=0;
        sleep(0.001);
        unsigned char *mem = static_cast<unsigned char*>(region.get_address());
        for(std::size_t i = 0; i < region.get_size(); ++i){
            if(*mem == 1)    
                c++ ;
            mem++;
        }
        //printf("c %d and n_proc %d\n",c,n_proc);
            
    }
    //printf("This should be printed at once? 5 times \n");
}

