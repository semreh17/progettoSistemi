#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"

extern int processCount;
extern volatile unsigned int globalLock;
extern struct pcb_t *currentProcess[NCPU];
extern struct list_head readyQueue;
extern void scheduler();
extern int deviceSemaphores[NRSEMAPHORES];

extern void klog_print();
extern void klog_print_dec();

#define BITMAP(IntlineNo) (*(memaddr *)(Cbitmap_BASE + ((IntlineNo) - 3) * WS))



void dev_interrupt_handler(int cause, state_t *statep) {


    int IntlineNo = 0;

    switch (cause)
    {   
        case IL_DISK:     IntlineNo = 3; break;
        case IL_FLASH:    IntlineNo = 4; break;
        case IL_ETHERNET: IntlineNo = 5; break;
        case IL_PRINTER:  IntlineNo = 6; break;
        case IL_TERMINAL: IntlineNo = 7; break;

        default: PANIC();
    }

    devregarea_t *dev_area = (devregarea_t *)RAMBASEADDR; //accesso all'area dei registri dispositivo

    memaddr bitmap = BITMAP(IntlineNo);

    unsigned int DevNo; //ricerca di DevNo per priority

    if (bitmap & DEV7ON) DevNo = 7;

    else if (bitmap & DEV6ON) DevNo = 6;

    else if (bitmap & DEV5ON) DevNo = 5;

    else if (bitmap & DEV4ON) DevNo = 4;

    else if (bitmap & DEV3ON) DevNo = 3;

    else if (bitmap & DEV2ON) DevNo = 2;

    else if (bitmap & DEV1ON) DevNo = 1;

    else DevNo = 0;

    memaddr devAddrBase = START_DEVREG + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10); //scritta così nelle spec
    unsigned int status;
    int index = 0;

    

    if (IntlineNo == 7){
        termreg_t *term_reg = (termreg_t *)devAddrBase; //salvare status e mandare ACK, leggendo da types.h mi sembra corretto ma boh
        status = term_reg->recv_status;
        term_reg->recv_command = ACK; // 4 | 4 | devAddrBase % 0x10 = 0 | 0x08
    } else {
        // Gestione generica per dispositivi non terminali
        dtpreg_t *devReg = (dtpreg_t *) devAddrBase;
        status = devReg->status;  // Salvataggio dello stato del dispositivo
        devReg->command = ACK;  // Acknowledge scrivendo ACK nel registro command
    }


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

        //Calcolo tempo utilizzato
   // current_process[getPRID()]->p_time += 0;   // calcoliamo il tempo di esecuzione
    currentProcess[getPRID()]->p_s = *statep;               // copiamo lo stato del processo corrente
    insertProcQ(&readyQueue, currentProcess[getPRID()]);     // mettiamo il processo corrente nella ready queue
    currentProcess[getPRID()] = NULL;
    RELEASE_LOCK(&globalLock);

    scheduler();    //Call the Scheduler.

}



static void SystemWideIntervalTimer(state_t *statep) {
    //1. Acknowledge the interrupt by loading the Interval Timer with a new value: 100 milliseconds
    //(constant PSECOND). Load the Interval Timer value can be done with the following pre-defined
    //macro LDIT(T).
    LDIT(PSECOND);
    // *((cpu_t *)INTERVALTMR) = PSECOND;
    ACQUIRE_LOCK(&globalLock);
    pcb_t *toUnblock = NULL;   //2. Unblock all PCBs blocked waiting a Pseudo-clock tick.
    while((toUnblock = removeBlocked(&deviceSemaphores[48])) != NULL) {
        insertProcQ(&readyQueue, toUnblock);
    }
    //3. Return control to the Current Process of the current CPU if exists: perform a LDST on the saved
    //exception state of the current CPU.

    RELEASE_LOCK(&globalLock);
    if(currentProcess[getPRID()] != NULL) {
        LDST(statep);
    } else {
        scheduler();
    }
}


void interruptHandler(int cause, state_t *statep) {

    if (cause == IL_TIMER) {
        klog_print("il dio bestia");
        SystemWideIntervalTimer(statep);
    } else if (cause == IL_CPUTIMER) {
        LocalTimerInterrupt(statep);
    } else {
        dev_interrupt_handler(cause, statep);
    }

    //
    // if (CAUSE_IP_GET(cause, IL_DISK))
    //     dev_interrupt_handler(IL_DISK, statep);
    //
    // else if (CAUSE_IP_GET(cause, IL_FLASH))
    //     dev_interrupt_handler(IL_FLASH, statep);
    //
    // else if (CAUSE_IP_GET(cause, IL_ETHERNET))
    //     dev_interrupt_handler(IL_ETHERNET, statep);
    //
    // else if (CAUSE_IP_GET(cause, IL_PRINTER))
    //     dev_interrupt_handler(IL_PRINTER, statep);
    //
    // else if (CAUSE_IP_GET(cause, IL_TERMINAL))
    //     dev_interrupt_handler(IL_TERMINAL, statep);
    //
    // else if (CAUSE_IP_GET(cause, IL_CPUTIMER))
    //     LocalTimerInterrupt(statep);
    //
    // else if (cause == IL_TIMER) {
    //     klog_print("LE BANANE");
    //     SystemWideIntervalTimer(statep);
    // }
}