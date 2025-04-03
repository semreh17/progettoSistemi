#include "../headers/types.h"
#include "uriscv/types.h"
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"
#include "uriscv/arch.h"

extern int processCount;         
extern int globalLock;
extern struct pcb_t *currentProcess[NCPU];         
extern struct list_head readyQueue;



#define BITMAP_BASE 0x10000040
#define CAUSE_EXCCODE_MASK 0x0000007F

void interrupt_handler() {
   
    ACQUIRE_LOCK(&globalLock);

//Tip: to figure out the current interrupt exception code, you can use a bitwise AND betweengetCAUSE() and the constants CAUSE_EXCCODE_MASK.
    unsigned int cause = getCAUSE();
    unsigned int exc_code = cause & CAUSE_EXCCODE_MASK;                                                         
    int intLineNo = 0;

    /*The Interrupting Devices Bit Map is a read-only five word area located starting from address
    0x1000.0040. Interrupting Devices Bit Map words have this format: when bit i in word j is set to
    one then device i attached to interrupt line j + 3 has a pending interrupt, see Table 3. An interrupt
    pending bit is turned on automatically by the hardware whenever a device’s controller asserts the
    interrupt line to which it is attached. The interrupt will remain pending –the pending interrupt bit
    will remain on– until the interrupt is acknowledged. Interrupts for peripheral devices are acknowledged
    by writing the acknowledge command code in the appropriate device’s device register. */

    
    switch(exc_code) {
        case 7:  intLineNo = 1; break;  // PLT (IL_CPU_TIMER)
        case 3:  intLineNo = 2; break;  // Interval Timer (IL_TIMER)
        case 17: intLineNo = 3; break;  // Disk (IL_DISK)
        case 18: intLineNo = 4; break;  // Flash (IL_FLASH)
        case 19: intLineNo = 5; break;  // Ethernet (IL_ETHERNET)
        case 20: intLineNo = 6; break;  // Printer (IL_PRINTER)
        case 21: intLineNo = 7; break;  // Terminal (IL_TERMINAL)
        default: goto CappoHaiSbagliato;   //errore nell'exception handler (si lo so il goto fa merda)   
    }



CappoHaiSbagliato:
    //non so se dovrebbe esserci altra roba qui
    RELEASE_LOCK(&globalLock);
}



static void localTimerInterruptHandler(state_t *exception_state) {  //per IL_CPU

    int ID=getPRID();

    ACQUIRE_LOCK(&globalLock); //non so quando ci va quindi io lo metto sempre fanculo
    

    setPLT(0); //nelle spec c'è scritto di usare setTIMER ma non ne vedo il motivo quando c'è questa(?)
    
    
    updateCPUtime(currentProcess[ID]);
    
    //Copy the processor state of the current CPU at the time of the exception into the Current
    //Process’s PCB (p_s) of the current CPU.

    saveState(&(currentProcess[ID]->p_s), exception_state);
    
    //Place the Current Process on the Ready Queue; transitioning the Current Process from the
    //“running” state to the “ready” state. (sto cambio di processo lo fa già la instertProcQ? perchè se non lo fa c'è da aggiungerlo)
    insertProcQ(&readyQueue, currentProcess[ID]);
    currentProcess[ID] = NULL;
    
    
    RELEASE_LOCK(&globalLock);
    
    
    scheduler(); //Call the Scheduler
}