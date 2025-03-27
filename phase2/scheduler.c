#include "./include/scheduler.h"  //che ovviamente non funziona dio animale

extern int processCount;         
extern int globalLock;            
extern struct list_head readyQueue;

void scheduler() {
    int core_id = getCPUID(); //robaccia di riscv
    
    while(globalLock);  // busy wiat per il lock
    globalLock = 1;

    pcb_t *new_process = removeProcQ(&readyQueue);  //rimuove processo dalla queue
    
    if(new_process != NULL) {
        
        currentProcess[core_id] = new_process;      //assegnazione al core corrente
        
        
        setPLT(TIMESLICE * (*((cpu_t *)TIMESCALEADDR))); //timer che non ho ancora capito come funzioni
        
        
        globalLock = 0;
        
        LDST(&(new_process->p_s));  //caricamento process state

    } else {
        
        if(processCount == 0) {     //se la coda è vuota
            globalLock = 0;
            HALT();
        }

        else if(processCount > 0 && soft_blocked_count > 0) {
            
            setSTATUS(getSTATUS() | IECON | IMON);
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



