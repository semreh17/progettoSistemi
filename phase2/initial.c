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
int deviceSemaphores[NRSEMAPHORES];
volatile unsigned int globalLock;
extern void test();
extern void scheduler();
extern void uTLB_RefillHandler();
extern void exceptionHandler();

// TODO: MODIFICA PASSUPVECTORINIT E CONFIGUREIRT

void passupvectorInit() {
    passupvector_t *passupvector = (passupvector_t *) PASSUPVECTOR;
    for (int i = 0; i < NCPU; i++) {
        passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
        if (i == 0) {
            passupvector->tlb_refill_stackPtr = KERNELSTACK;
            passupvector->exception_stackPtr = KERNELSTACK;
        } else {
            passupvector->tlb_refill_stackPtr = RAMSTART + (64 * PAGESIZE) + (i * PAGESIZE);
            passupvector->exception_stackPtr = 0x20020000 + (i * PAGESIZE);
        }
        passupvector->exception_handler = (memaddr)exceptionHandler;
        passupvector++; // qui avevamo sbagliato, dovevamo semplicemente incrementare il puntatore
    }
}
void configureIRT(int line, int cpu) {
    for (int line = 0; line < N_INTERRUPT_LINES; line++) {  // dobbiamo farlo per ogni linea di interruzione
            for (int dev = 0; dev < N_DEV_PER_IL; dev++) {        // per ogni device
                memaddr *irt_entry = (memaddr *)IRT_ENTRY(line, dev);
                *irt_entry = IRT_RP_BIT_ON;
                for (int cpu_id = 0; cpu_id < NCPU; cpu_id++){
                    *irt_entry |= (1U << cpu_id); // setta il bit di destinazione, 1 << cpu ...001 | ...010 | ...100; in fase finale era sbagliata prima
                }
            }
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
    pcb_t *firstProcess = allocPcb();
    firstProcess->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    firstProcess->p_s.mie = MIE_ALL;
    firstProcess->p_s.pc_epc = (memaddr)test;
    RAMTOP(firstProcess->p_s.reg_sp);
    insertProcQ(&readyQueue, firstProcess);
    processCount++;
    klog_print("test\n");

    // 7.
    // vengono assegnati 6 registri a ogni CPU
    unsigned int bits = 0;
    for (int cpu = 0; cpu < NCPU; cpu++)
        bits += (1 << cpu);
    for (int i = 0; i < IRT_NUM_ENTRY; i++) {
        memaddr entry = IRT_START + i*WS;
        *((memaddr *)entry) = 0; // just to be sure that the entry is 0 before initializing it
        *((memaddr *)entry) |= IRT_RP_BIT_ON;
        *((memaddr *)entry) |= bits;
    }

    *((memaddr *)TPR) = 0;
    klog_print("registri gas\n");

    // 8.
    state_t stato;
    stato.status = MSTATUS_MPP_M;
    stato.pc_epc = (memaddr) scheduler;
    stato.entry_hi = 0;
    stato.cause = 0;
    stato.mie = 0;

    for (int i = 1; i < NCPU; i++) {
        stato.reg_sp = (0x20020000 + (i * PAGESIZE));
        INITCPU(i, &stato);
    }

    if (currentProcess[0] != NULL) klog_print(" PROCESSO NON VUOTO ");

    klog_print("SCHEDULER TUTTO OK\n");
    scheduler();

}
