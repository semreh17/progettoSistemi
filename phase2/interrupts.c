#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"

extern int processCount;
extern volatile unsigned int globalLock;
extern struct pcb_t *currentProcess[NCPU];
extern struct list_head readyQueue;
extern void scheduler();
extern int deviceSemaphores[NRSEMAPHORES];

void dev_interrupt_handler(int IntLineNo, state_t *statep) {

    devregarea_t *dev_area = (devregarea_t *)RAMBASEADDR; //accesso all'area dei registri dispositivo

    unsigned int dev_bitmap = dev_area->interrupt_dev[IntLineNo - 3];

    unsigned int DevNo; //ricerca di DevNo per priority
    if (dev_bitmap & DEV7ON) DevNo = 7;

    else if (dev_bitmap & DEV6ON) DevNo = 6;

    else if (dev_bitmap & DEV5ON) DevNo = 5;

    else if (dev_bitmap & DEV4ON) DevNo = 4;

    else if (dev_bitmap & DEV3ON) DevNo = 3;

    else if (dev_bitmap & DEV2ON) DevNo = 2;

    else if (dev_bitmap & DEV1ON) DevNo = 1;

    else DevNo = 0;

    memaddr devAddrBase = START_DEVREG + ((IntLineNo - 3) * 0x80) + (DevNo * 0x10); //scritta così nelle spec
    unsigned int status;
    int index = 0;
    if (IntLineNo == 7){
        termreg_t *term_reg = (termreg_t *)devAddrBase; //salvare status e mandare ACK, leggendo da types.h mi sembra corretto ma boh
        status = term_reg->recv_status;
        term_reg->recv_command = ACK; // 4 | 4 | devAddrBase % 0x10 = 0 | 0x08
    } else {
        // Gestione generica per dispositivi non terminali
        dtpreg_t *devReg = (dtpreg_t *) devAddrBase;
        status = devReg->status;  // Salvataggio dello stato del dispositivo
        devReg->command = ACK;  // Acknowledge scrivendo ACK nel registro command
    }
    // verhogen(*statep); //perform a V operation on the Nucleus maintained semaphore associated with this (sub)device.(???)

    pcb_t *toUnblock = removeBlocked(&deviceSemaphores[index]);
    if (toUnblock != NULL) {
        toUnblock->p_s.reg_a0 = status; //qua va bene o dovrei usare memcpy?
        
        insertProcQ(&readyQueue, toUnblock);
    }


    if (currentProcess[getPRID()]) {
        LDST(statep);
    } else {
        scheduler();
    }

}



static void LocalTimerInterrupt(state_t *statep) {  //per IL_CPUTIMER

    ACQUIRE_LOCK(&globalLock);

    setTIMER(TIMESLICE * (*(cpu_t *)TIMESCALEADDR)); // non va

    pcb_t *current_pcb = currentProcess[getPRID()];
        //Calcolo tempo utilizzato
   // current_process[getPRID()]->p_time += 0;   // calcoliamo il tempo di esecuzione
    currentProcess[getPRID()]->p_s = *statep;               // copiamo lo stato del processo corrente
    insertProcQ(&readyQueue, currentProcess[getPRID()]);     // mettiamo il processo corrente nella ready queue
    currentProcess[getPRID()] = NULL;
    RELEASE_LOCK(&globalLock);

    scheduler();    //Call the Scheduler.

}



static void SystemWideIntervalTimer(state_t *ExeptionOccurred) {
    //1. Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
    //(constant PSECOND). Load the Interval Timer value can be done with the following pre-defined
    //macro LDIT(T).
    LDIT(PSECOND);
    ACQUIRE_LOCK(&globalLock);


    pcb_t *toUnblock;   //2. Unblock all PCBs blocked waiting a Pseudo-clock tick.

    while((toUnblock = removeBlocked(&deviceSemaphores[48])) != NULL) {
        insertProcQ(&readyQueue, toUnblock);

    }

    //3. Return control to the Current Process of the current CPU if exists: perform a LDST on the saved
    //exception state of the current CPU.

    if(currentProcess[getPRID()]) {
        RELEASE_LOCK(&globalLock);
        LDST(&currentProcess[getPRID()]->p_s);

    } else {
        RELEASE_LOCK(&globalLock);
        scheduler();
    }
}


void interruptHandler(int cause, state_t *statep) {

    if (CAUSE_IP_GET(cause, IL_DISK))
        dev_interrupt_handler(IL_DISK, statep);

    else if (CAUSE_IP_GET(cause, IL_FLASH))
        dev_interrupt_handler(IL_FLASH, statep);

    else if (CAUSE_IP_GET(cause, IL_ETHERNET))
        dev_interrupt_handler(IL_ETHERNET, statep);

    else if (CAUSE_IP_GET(cause, IL_PRINTER))
        dev_interrupt_handler(IL_PRINTER, statep);

    else if (CAUSE_IP_GET(cause, IL_TERMINAL))
        dev_interrupt_handler(IL_TERMINAL, statep);

    else if (CAUSE_IP_GET(cause, IL_CPUTIMER))
        LocalTimerInterrupt(statep);

    else if (CAUSE_IP_GET(cause, IL_TIMER))
        SystemWideIntervalTimer(statep);

    //else ERROR(?)
}