#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"  // Per le funzioni e variabili di uriscv
#include "uriscv/arch.h"
#define CAUSE_EXCCODE_MASK 0x0000007F


// Roba globale che serve presa dallo scheduler
extern int processCount;         // quanti processi ci sono in giro
extern struct pcb_t *currentProcess[NCPU];  // Chi sta girando su ogni core
extern struct list_head readyQueue;  // La fila dei processi pronti


void exceptionHandler() {
    int core_id = getPRID();  //funzione strana di uriscv per prendere l'ID del core

    state_t *exc_state = GET_EXCEPTION_STATE_PTR(core_id);    // Prendiamo lo stato del processo che ha generato l'eccezione
    
    unsigned int cause = getCAUSE(); //funzione di uriscv per prendere la causa dell'eccezione
    
    if (CAUSE_IS_INT(cause)) {    //controlla solo il bit più significativo (bit 31), se 1 interrupt se 0 eccezione
        handleInterrupt();  
        return;
    }
    
    unsigned int exc_code = cause & CAUSE_EXCCODE_MASK; //CAUSE_EXCCODE_MASK è una maschera per prendere i bit 0-5 della causa dell'eccezione

    switch(exc_code) {
        case 8:  // SYSCALL
        case 11: // SYSCALL
            handleSyscall(exc_state);  // il gestore di syscall
            break;
            
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
            PANIC();  // per ora scrivo solo panic perché serve una parte dopo 
            break;
            
        default: // tutto gli altri casi (Program Trap)
            PANIC();  // altro panic
            break;
    }
}


void handleInterrupt() {
    //  Qui ci va la logica per capire se è il PLT (timer locale),se sì, reinserire il processo in ready queue e chiamare lo scheduler
    //  se è l'interval time fare il tick del pseudo-clock
    //  se è un dispositivo svegliare chi stava aspettando
    //  che a questo punto scriviamo direttamente in interrupt.c
    
    LDST(GET_EXCEPTION_STATE_PTR(getPRID()));
}


void uTLB_RefillHandler() {         //fuzione fornita neelle spec
int prid = getPRID();              
5
setENTRYHI(0x80000000);
setENTRYLO(0x00000000);
TLBWR();
LDST(GET_EXCEPTION_STATE_PTR(prid));
}