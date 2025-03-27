#include "./include/scheduler.h"

extern int processCount;          
extern int globalLock;             
extern struct list_head readyQueue;

void scheduler() {
    int core_id = getCPUID(); //robaccia di μRISC-V
    
    
    while(globalLock);  // busy-wait per il lock
    globalLock = 1;

    
    pcb_t *new_process = removeProcQ(&readyQueue); //rimozione processo dalla ready Queue
    
    if(new_process != NULL) {
        
        currentProcess[core_id] = new_process;
        
        
        setPLT(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));// ancora non ho capito sti timer, ma dovrebbe funzionare
        
        
        globalLock = 0;
        
        
        LDST(&(new_process->p_s));  //caricamento process state
    } else {
        
        if(processCount == 0) { //estione coda vuota (usa processCount da initial)
            globalLock = 0;
            HALT();
        }
        else if(processCount > 0 && soft_blocked_count > 0) {
            
            setSTATUS(getSTATUS() | IECON | IMON);  //config inteerupt
            setMIE(getMIE() & ~0x80); //disabilita timer interrupt
            
            currentProcess[core_id] = NULL;
            globalLock = 0;
            WAIT();
        }
        else {
            globalLock = 0;
            PANIC();
        }
    }
}


