#include "./include/scheduler.h"  //che ovviamente non funziona dio animale

extern int processCount;         
extern int globalLock;            
extern struct list_head readyQueue;

void scheduler() {
    int core_id = getPRID(); //robaccia di riscv
    
    ACQUIRE_LOCK();

    pcb_t *new_process = removeProcQ(&readyQueue);  //rimuove processo dalla queue
    
    if(new_process != NULL) {
        
        currentProcess[core_id] = new_process;      //assegnazione al core corrente
        
        new_process->p_s.cause = 0;  
        new_process->p_s.status &= ~0x800; 
        
        setPLT(TIMESLICE * (*((cpu_t *)TIMESCALEADDR))); //timer che non ho ancora capito come funzioni
        
        RELEASE_LOCK();
        
        LDST(&(new_process->p_s));  //caricamento process state

    } else {
        
        if(processCount == 0) {     //se la coda è vuota
            RELEASE_LOCK();
            HALT();
        }

        else if(processCount > 0 && soft_blocked_count > 0) {
            
            setTPR(1);      //tpr=task priority manager
            
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);  //disabilita timer interrupt
            
            currentProcess[core_id] = NULL;
            RELEASE_LOCK();
            WAIT();
        }

        else {

            RELEASE_LOCK();
            PANIC();
        }
    }
}