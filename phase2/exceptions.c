#include "../phase1/headers/pcb.h"
extern int processCount;         // quanti processi ci sono in giro
extern pcb_t *currentProcess[NCPU];  // Chi sta girando su ogni core
extern struct list_head readyQueue;  // La fila dei processi pronti

void handleInterrupt() {
    //  Qui ci va la logica per capire se è il PLT (timer locale),se sì, reinserire il processo in ready queue e chiamare lo scheduler
    //  se è l'interval time fare il tick del pseudo-clock
    //  se è un dispositivo svegliare chi stava aspettando
    //  che a questo punto scriviamo direttamente in interrupt.c

    LDST(GET_EXCEPTION_STATE_PTR(getPRID()));
}

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
            // handleSyscall(exc_state);  // il gestore di syscall
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

void terminateProcess(int pid) {
    // creo un pcb vuoto che conterrà il pcb effettivo
    pcb_t* toTerminate;
    toTerminate->p_pid = pid;
    // qua sborro (con container_of assegno il pcb con quell'effettivo pid a toTerminate)
    toTerminate = container_of(&toTerminate->p_list, pcb_t, p_pid);
    // scorro (sempre sborrando) finchè toTerminate non ha più figli, rimuovendoli ad ogni iterata
    while (emptyChild(toTerminate)) {
        outChild(toTerminate);
    }
    // infine termino, rimuovendolo dalla readyQueue (sperando che sia effettivamente lì)
    removeProcQ(&toTerminate->p_list);
    processCount--;
}

void passeren(int *semAdd) {
    /*
     * se semAdd = 1 allora decrementa il semaforo e fai una LDST sul pcb bloccato su quel semaforo
     * in caso contrario devi fermare il processo su quel semaforo in qualche modo (con una wait? boh)
     */
}
