#include "../headers/types.h"
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"

extern int processCount;
extern volatile unsigned int globalLock;
extern struct list_head readyQueue;
extern struct pcb_t *currentProcess[NCPU];

void scheduler() {
    unsigned int coreId = getPRID();

    if (!globalLock) {
        ACQUIRE_LOCK(&globalLock);
    }

    pcb_t *newProcess = removeProcQ(&readyQueue);  //rimuove processo dalla queue

    if (!emptyProcQ(&readyQueue)) {
        // qua probably manca della roba
        currentProcess[coreId] = newProcess;      //assegnazione al core corrente
        RELEASE_LOCK(&globalLock);
        LDST(&(newProcess->p_s));  //caricamento process state
    }else {
        if (processCount == 0) {     //se la coda è vuota
            RELEASE_LOCK(&globalLock);
            HALT();
        }else if (processCount > 0 && coreId != 0) { // non ho capito perché controlli anche il coreId

            *((memaddr *)TPR) = 1; // setting the TPR to 1
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);  // disabilita timer interrupt

            //currentProcess[coreId] = NULL; la commento, ma non so in realtà a cosa serva
            RELEASE_LOCK(&globalLock);
            WAIT();
        }
    }
}
#include "../headers/types.h"
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"

void exceptionHandler() {}

int createProcess(state_t *statep, pcb_t *caller) {
    pcb_t *newProcess = allocPcb();
    if (newProcess == NULL) {
        statep->gpr[24] = -1;
        return NULL;
    }

    newProcess->p_s = * (state_t*)statep->gpr[25];
    newProcess->p_supportStruct = (support_t*)statep->gpr[26];
    newProcess->p_pid = caller->p_pid++; // uh?

}