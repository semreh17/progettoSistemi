#include "../headers/types.h"
#include "uriscv/types.h"
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"

int processCount; // started but not terminated process
struct list_head *readyQueue; // queue of PCB's in the ready state

pcb_t *currentProcess[NCPU]; // vector of pointers to the PCB that is in the "running" state on each CPU
volatile int globalLock;

semd_t deviceSemaphores[NRSEMAPHORES];
//device semaphores: two types semaphores, the first for each external device and the other to support the Pseudo-clock

extern void test(); // function/proces to test the nucleus

void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST(GET_EXCEPTION_STATE_PTR(prid));
}

void exceptionHandler() {} // just a placeholder, has to be implemented

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

    globalLock = 1;
    processCount = MAXPROC;
    mkEmptyProcQ(readyQueue);
    for (int i = 0; i < NCPU; i++) {
        currentProcess[i] = allocPcb();
    }

    /*
       *
       * deviceSemaphores (???)
       *
    */
    for (int i = 0; i < NRSEMAPHORES; i++) {

    }

    // 5.
    LDIT(PSECOND);

    // 6.
    /*
     * probably not needed to set manually the tree fields to NULL since allocPcb() already does that
     * actually it's not really needed to set anything to NULL/0 since allocPcb() does already everything
     */
    pcb_t *kernel = allocPcb();
    kernel->p_parent = NULL;
    kernel->p_time = 0;
    kernel->p_semAdd = NULL;
    kernel->p_supportStruct = NULL;

    // 7. uhhhhh????

    // 8. uh?
    for (int i = 0; i < NCPU - 1; i++) {

    }



}
