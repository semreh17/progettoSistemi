#include "../headers/types.h"
#include "uriscv/types.h"
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"
#include "uriscv/arch.h"

void *scheduler(); // placeholder for scheduler

int processCount; // started but not terminated process
struct list_head *readyQueue; // queue of PCB's in the ready state

struct pcb_t *currentProcess[NCPU]; // vector of pointers to the PCB that is in the "running" state on each CPU
volatile int globalLock;

struct semd_t deviceSemaphores[NRSEMAPHORES];
//device semaphores: two types semaphores, the first for each external device and the other to support the Pseudo-clock

extern void test(); // function/process to test the nucleus

extern void uTLB_RefillHandler();

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

    // 3.
    initPcbs();
    initASL();

    // 4.
    globalLock = 0;
    processCount = 0;
    mkEmptyProcQ(readyQueue);
    for (int i = 0; i < NCPU; i++) {
        currentProcess[i] = NULL;
    }
    for (int i = 0; i < NRSEMAPHORES; i++) {
        deviceSemaphores[i] = {0};
    }

    // 5.
    LDIT(PSECOND);

    // 6.
    /*
     * enabling interrupts, setting kernel mode on and SP to RAMTOP
     */
    pcb_t *kernel = currentProcess[0];
    kernel = allocPcb();
    kernel->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    RAMTOP(kernel->p_s.reg_sp);
    kernel->p_s.mie = MIE_ALL;
    kernel->p_s.pc_epc = (memaddr)test;
    processCount++;
    insertProcQ(readyQueue, kernel);

    // 7.
    // vengono assegnati 6 registri ad ogni CPU
    int cpu_counter = -1;
    for (int i = 0; i < IRT_NUM_ENTRY; i++) {
        if (i % (IRT_NUM_ENTRY/NCPU) == 0) { // quando finisce di riempire tutti e 6 i registri di ogni CPU allora incrementa il cpu_counter
            cpu_counter++;
        }
        *((memaddr *)(IRT_START + i*WS)) |= IRT_RP_BIT_ON; // il 28esimo bit della interrupt line viene settato ad 1
        *((memaddr *)(IRT_START + i*WS)) |= (1 << cpu_counter); // il CPU-esimo bit dell'interrupt line viene settato ad 1
    }

    // 8. uh?
    for (int i = 1; i < NCPU; i++) {
        currentProcess[i] = allocPcb();
        currentProcess[i]->p_s.status = MSTATUS_MPP_M;
        currentProcess[i]->p_s.pc_epc = (memaddr)scheduler;
        currentProcess[i]->p_s.reg_sp = 0x20020000 + (i * PAGESIZE);

    }



}
