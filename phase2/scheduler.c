#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/pcb.h"

extern int processCount;         
extern int globalLock;
extern struct pcb_t *currentProcess[NCPU];         
extern struct list_head readyQueue;

void scheduler() {
    int core_id = getPRID(); //robaccia di riscv
    
    ACQUIRE_LOCK(&globalLock);

    pcb_t *new_process = removeProcQ(&readyQueue);  //rimuove processo dalla queue
    
    if(new_process != NULL) {
        
        currentProcess[core_id] = new_process;      //assegnazione al core corrente
        
        setPLT(TIMESLICE * (*((cpu_t *)TIMESCALEADDR))); //timer che non ho ancora capito come funzioni
        
        RELEASE_LOCK();
        
        LDST(&(new_process->p_s));  //caricamento process state

    } else {
        
        if(processCount == 0) {     //se la coda è vuota
            RELEASE_LOCK();
            HALT();
        }

        else if(processCount > 0 && core_id!=0) {
            
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