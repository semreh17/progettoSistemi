<<<<<<< HEAD
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
=======
#include "../headers/types.h"
#include "uriscv/types.h"
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"
#include "uriscv/arch.h"
#include "scheduler.h"

extern int globalLock; //0=libero, 1=occupato

// credo che qui ci si riferisca alla variabile currentProcess[] in initil.c ma non ne sono sicuro (da chiarire)
extern pcb_t* per_cpu_current_process[NCPU]; // Array per-CPU

void scheduler() {

    while(swap(&globalLock, 1));   //no idea, me lo ha detto deep

    // Rimozione dalla Ready Queue
    pcb_t* current_process = removeProcQ(&Ready_Queue);

    if(current_process != NULL) {

        int cpu_id = getCPUID();   // Impostazione current process per questo core
        per_cpu_current_process[cpu_id] = current_process;

        setPLT(TIMESLICE * (*((cpu_t*)TIMESCALEADDR)));

        globalLock = 0;

        LDST(&(current_process->p_s));
    }

    else {

        if(process_count == 0) {  //coda vuota
            globalLock = 0; //core libero
            HALT();
        }
        else if(process_count > 0 && soft_blocked_count > 0) {


            unsigned int status = getSTATUS();


            per_cpu_current_process[getCPUID()] = NULL;


            globalLock = 0;         // Rilascio lock prima di WAIT
>>>>>>> 5cc7a02bc40f5df1aeeef2a45a8e8d41fcc658f7
            WAIT();
        }
        else {
            globalLock = 0;
            PANIC();
        }
    }
}


