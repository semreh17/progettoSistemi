
// Roba globale che serve presa dallo scheduler
extern int processCount;         // quanti processi ci sono in giro
extern struct pcb_t *currentProcess[NCPU];  // Chi sta girando su ogni core
extern struct list_head readyQueue;  // La fila dei processi pronti
extern volatile unsigned int globalLock;

void interrupt_handlerr() {
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
        interrupt_handler();  
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


void handleSyscall(state_t *exc_state) {
    int core_id = getPRID();
    pcb_t *current = currentProcess[core_id];
    int syscall_code = exc_state->reg_a0;
    int result = 0;

    // 1. Controllo kernel mode
    if (!(exc_state->status & MSTATUS_MPP_MASK)) {
        exc_state->cause = PRIVINSTR;
        programTrapHandler();
        return;
    }

    ACQUIRE_LOCK(&globalLock);

    switch(syscall_code) {
        // 2. CREATEPROCESS (-1)
        case -1: {
            state_t *new_state = (state_t *)exc_state->reg_a1;
            support_t *support = (support_t *)exc_state->reg_a3;
            
            pcb_t *new_pcb = allocPcb();
            if (new_pcb == NULL) {
                result = -1;
                break;
            }

            new_pcb->p_s = *new_state;
            new_pcb->p_supportStruct = support;
            new_pcb->p_time = 0;
            new_pcb->p_semAdd = NULL;

            insertProcQ(&readyQueue, new_pcb);
            insertChild(current, new_pcb);
            processCount++;
            result = new_pcb->p_pid;
            break;
        }

        // 3. TERMINATEPROCESS (-2)
        case -2: {
            int pid_to_kill = exc_state->reg_a1;
            pcb_t *target = (pid_to_kill == 0) ? current : NULL; // Implementa findPcbByPid se necessario

            if (target != NULL) {
                terminateProcessTree(target);
                processCount -= countTerminatedProcesses();
            }
            RELEASE_LOCK(&globalLock);
            scheduler();
            return;
        }
    }
}
