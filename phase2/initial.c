#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include <uriscv/types.h>
#include <uriscv/cpu.h>
#include <uriscv/arch.h>
#include "p2test.c"
#include "scheduler.c"
#include "interrupts.c"
#include "exceptions.c"
#include "../klog.c"


int processCount; // started but not terminated process
struct list_head readyQueue; // queue of PCB's in the ready state
struct pcb_t *currentProcess[NCPU]; // vector of pointers to the PCB that is in the "running" state on each CPU
volatile unsigned int globalLock;
int deviceSemaphores[NRSEMAPHORES];

extern void test();
extern void scheduler();
extern void uTLB_RefillHandler();
extern void exceptionHandler();

void passupvectorInit() {
    passupvector_t *passup = PASSUPVECTOR;

    // init for the pass up vector of the first CPU
    passup->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passup->tlb_refill_stackPtr = KERNELSTACK;
    passup->exception_handler = (memaddr)exceptionHandler;
    passup->exception_stackPtr = KERNELSTACK;

    // init of the processors from 1 to 7, i
    //i represents the cpu_id mentioned in the specs
    for (int i = 1; i <= NCPU; i++) {
        passup = PASSUPVECTOR + (i*0x10);
        // passup++;
        passup->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
        passup->tlb_refill_stackPtr = RAMSTART + (64 * PAGESIZE) +(i * PAGESIZE);
        passup->exception_handler = (memaddr)exceptionHandler;
        passup->exception_stackPtr = RAMSTART + (64 * PAGESIZE) +(i * PAGESIZE);
    }
}

int main() {
    // 2.
    passupvectorInit();
    klog_print("PASSUPVECTOR INITIALIZED\n");

    // 3.
    initPcbs();
    initASL();
    klog_print("pcb e asl INITIALIZED\n");

    // 4.
    globalLock = 0;
    processCount = 0;
    mkEmptyProcQ(&readyQueue);
    for (int i = 0; i < NCPU; i++) {
        currentProcess[i] = allocPcb();
    }
    for (int i = 0; i < NRSEMAPHORES; i++) {
        deviceSemaphores[i] = 1;
    }
    klog_print("semafroc INITIALIZED\n");

    // 5.
    LDIT(PSECOND);
    klog_print("LDIT INITIALIZED\n");


    // 6.
    // enabling interrupts, setting kernel mode on and SP to RAMTOP
    pcb_t *kernel = *currentProcess;
    kernel = allocPcb();
    kernel->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    RAMTOP(kernel->p_s.reg_sp);
    kernel->p_s.mie = MIE_ALL;
    kernel->p_s.pc_epc = (memaddr)test;
    processCount++;
    insertProcQ(&readyQueue, kernel);
    klog_print("test\n");

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
    *((memaddr *)TPR) = 0; // setting the task priority
    klog_print("registri gas\n");

    // 8.
    for (int i = 1; i < NCPU; i++) {
        currentProcess[i]->p_s.status = MSTATUS_MPP_M;
        currentProcess[i]->p_s.pc_epc = (memaddr)scheduler;
        currentProcess[i]->p_s.reg_sp = 0x20020000 + (i * PAGESIZE);
        currentProcess[i]->p_s.mie = 0;
        for (int j = 0; j < STATE_GPR_LEN; j++) {
            currentProcess[i]->p_s.gpr[j] = 0;
        }
        currentProcess[i]->p_s.cause = 0;
        currentProcess[i]->p_s.entry_hi = 0;
        INITCPU(i, &currentProcess[i]->p_s.status);
    }
    klog_print("SCHEDULER TUTTO OK\n");
    scheduler();

}
