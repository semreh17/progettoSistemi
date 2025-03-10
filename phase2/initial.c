#include <stdint.h>
#include <stdio.h>
#include "../headers/types.h"
#include "uriscv/types.h"
#include "../headers/const.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"

extern int processCount; // started but not terminated process
extern struct list_head readyQueue; // queue of PCB's in the ready state

extern pcb_t *currentProcess[MAXPROC]; // vector of pointers to the PCB that is in the "running" state on each CPU
static int globalLock;

//device semaphores: two types semaphores, the first for each external device and the other to support the Pseudo-clock


void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST(GET_EXCEPTION_STATE_PTR(prid));
}

void exceptionHandler() {} // placeholder to put in the exception file

void passupvectorInit() {
    passupvector_t *passup;

    // init for the pass up vector of the first CPU
    passup = PASSUPVECTOR;
    passup->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passup->tlb_refill_stackPtr = KERNELSTACK;
    passup->exception_handler = (memaddr)exceptionHandler;
    passup->exception_stackPtr = KERNELSTACK;

    // init of the processors from 1 to 7, i
    //i represents the cpu_id mentioned in the specs
    for (int i = 1; i <= NCPU; i++) {
        passup += 0x10;
        passup->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
        passup->tlb_refill_stackPtr = 0x20020000 + (i * PAGESIZE);
        passup->exception_handler = (memaddr)exceptionHandler;
        passup->exception_stackPtr = 0x20020000 + (i * PAGESIZE);
    }

}

int main() {
    passupvectorInit();

    initPcbs();
    initASL();

    // 4.
    /*
     * processCount = 20
     * readyQueue = LIST_HEAD_INIT(readyQueue)
     * currentProcess has to be initialized all to 0
     * deviceSemaphores (???)
     * globalLOck = brother uhhhhhh?????
     */
}
