#include "../../headers/types.h"    //dio porco sti path 
#include "uriscv/types.h"           //sto per ammazzare la mia famiglia
#include "../../headers/listx.h"          //qualcuno mi aiuti vi prego
#include "../../headers/const.h"
#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"
#include "uriscv/arch.h"

#define NCPU 8 
#define PAGESIZE 0x1000


void timer_handler() {      //necessario per i multi core, è un timer da richiamare per ogni core non saprei bene dove
    int core_id = getCPUID();
    
    
    while(globalLock);
    globalLock = 1;
    
    
    if(currentProcess[core_id] != NULL) {
        insertProcQ(&readyQueue, currentProcess[core_id]);
        currentProcess[core_id] = NULL;
    }
    
    
    globalLock = 0;
    scheduler();
}