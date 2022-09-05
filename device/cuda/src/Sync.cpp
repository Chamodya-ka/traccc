#include "traccc/cuda/utils/Sync.hpp"
#include <iostream>

namespace Sync{

void init_shared_mem(int n_proc, int u_id,int region_size,unsigned char* mem){
    Sync::n_proc=n_proc;
    Sync::u_id=u_id;
    Sync::region_size=region_size;
    printf("u_id : %d \n",Sync::u_id);
    unsigned char zero = 0x00;
    std::memset(mem+u_id, zero, sizeof (zero));
}


void reset_shared_mem(unsigned char* mem){
        mem[u_id]=0x00;
}


int get_uid(){
    return u_id;
}

void complete(unsigned char* mem){

    mem[u_id]=0x01;

    for(std::size_t i = 0; i < region_size; ++i){
        printf("index %d %d\n",i,*mem);
        mem++;
    }
}

void wait_for_reset(unsigned char* mem){
    int c = 0;
    while (c<n_proc){
        unsigned char* temp_mem = mem;
        c=0;
        for(std::size_t i = 0; i < region_size; ++i){
            if(*temp_mem == 0)   
                c++ ;
            temp_mem++;
        }
            
    }
    sleep(1);
}

void wait_for_other_processes(unsigned char* mem){

    
    int c = 0;
    while (c<n_proc){
        unsigned char* temp_mem = mem;
        c=0;
        for(std::size_t i = 0; i < region_size; ++i){
            if(*temp_mem == 1)   
                c++ ;
            temp_mem++;
        }
            
    }
    sleep(1);
}


};

