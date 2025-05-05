#include "../phase1/headers/pcb.h"
extern int processCount;         // quanti processi ci sono in giro
extern pcb_t *currentProcess[NCPU];  // Chi sta girando su ogni core
extern struct list_head readyQueue;  // La fila dei processi pronti
extern struct list_head semd_h;


void handleInterrupt() {
    int core_id = getPRID();
    state_t *exc_state = GET_EXCEPTION_STATE_PTR(core_id);
    unsigned int cause = getCAUSE();
    unsigned int exc_code = cause & CAUSE_EXCCODE_MASK;

    ACQUIRE_LOCK(&globalLock);

    // 1. Process Local Timer (PLT) - Codice 7 (IL_CPUTIMER)
    if (exc_code == IL_CPUTIMER) {
        if (currentProcess[core_id]) {
            // Salva stato corrente nel PCB
            currentProcess[core_id]->p_s = *exc_state;
            
            // Aggiorna tempo CPU usando campo p_time esistente
            cpu_t now;
            STCK(now);
            currentProcess[core_id]->p_time += (now - *((cpu_t *)TODLOADDR)) / (*((cpu_t *)TIMESCALEADDR));
            
            // Reinserisce in ready queue
            insertProcQ(&readyQueue, currentProcess[core_id]);
            currentProcess[core_id] = NULL;
        }
        
        // Resetta timer e chiama scheduler
        setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
        RELEASE_LOCK(&globalLock);
        scheduler();
        return;
    }
    
    // 2. Interval Timer - Codice 3 (IL_TIMER)
    if (exc_code == IL_TIMER) {
        // Resetta timer a 100ms (PSECOND)
        *((cpu_t *)INTERVALTMR) = PSECOND;
        
        // Sblocca processi in attesa usando SEMDEVLEN-1 come ID
        pcb_t *unblocked;
        while ((unblocked = removeBlocked((memaddr)(SEMDEVLEN-1))) != NULL) {
            insertProcQ(&readyQueue, unblocked);
        }
        
        RELEASE_LOCK(&globalLock);
        LDST(exc_state);
        return;
    }
    
    // 3. Interrupt I/O - Codici 17-21 (IL_DISK a IL_TERMINAL)
    if (exc_code >= IL_DISK && exc_code <= IL_TERMINAL) {
        int dev_line = exc_code - IL_DISK + 3;
        int *dev_addr = (int *)(START_DEVREG + (dev_line-3)*0x80);
        
        // Acknowledge interrupt
        *dev_addr = RECEIVEMSG;
        
        // Sblocca processo in attesa
        pcb_t *unblocked = removeBlocked((memaddr)dev_addr);
        if (unblocked) {
            unblocked->p_s.gpr[4] = *dev_addr; // a0 = status
            insertProcQ(&readyQueue, unblocked);
        }
        
        RELEASE_LOCK(&globalLock);
        LDST(exc_state);
        return;
    }
    
    RELEASE_LOCK(&globalLock);
    PANIC();
}

void exceptionHandler() {
    int core_id = getPRID();  //funzione strana di uriscv per prendere l'ID del core

    state_t *exc_state = GET_EXCEPTION_STATE_PTR(core_id);    // Prendiamo lo stato del processo che ha generato l'eccezione

    unsigned int cause = getCAUSE(); //funzione di uriscv per prendere la causa dell'eccezione

    if (CAUSE_IS_INT(cause)) {    //controlla solo il bit più significativo (bit 31), se 1 interrupt se 0 eccezione
        klog_print("qua dentro entra in un bel loooooooop");

        handleInterrupt();
        return;
    }

    unsigned int exc_code = cause & CAUSE_EXCCODE_MASK; //CAUSE_EXCCODE_MASK è una maschera per prendere i bit 0-5 della causa dell'eccezione

    switch(exc_code) {
        case 8:  // SYSCALL
        case 11: // SYSCALL
            // handleSyscall(exc_state);  // il gestore di syscall
            int syscall_num = exc_state->reg_a0;
            switch (syscall_num) {
            case TERMPROCESS:    // -2
                terminateProcess(exc_state);
                return;  // Non ritornare al processo
            
            case PASSEREN:       // -3
                passeren(exc_state);
                return;
            
            case VERHOGEN:       // -4
                verhogen(exc_state);
                return;
            
            case DOIO:           // -5
                // Implementa operazione I/O... credo in te Hermes
                return;
            
            case GETTIME:        // -6
                getCPUTime(exc_state);
                break;
            
            case CLOCKWAIT:      // -7
                waitForClock(exc_state);
                return;
            
            case GETSUPPORTPTR:  // -8
                getSupportData(exc_state);
                break;
            
            case GETPROCESSID:   // -9
                getProcessID(exc_state);
                break;
            
            default:
                PANIC();  // Syscall non riconosciuta
                return;
        }
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

/*
 * location value:
 * 0 = Current Process
 * 1 = Ready Queue
 * 2 = blocked on a semaphore
 */
void recursiveKiller(pcb_t *p, unsigned int location) {
    if (p == NULL) {
        return;
    }
    // rendo orfani i processi figli
    while (!emptyChild(p)) {
        pcb_t *child = container_of(&p->p_child, pcb_t, p_list);
        outChild(child);
        recursiveKiller(child, location);
    }
    // c'è da uccidere il processo a tutti gli effetti
    // teoricamente mi basta togliere ogni tipo di riferimento al processo, di conseguenza nulla lo punterà più
    unsigned int processorID = getPRID();
    switch (location) {
        case 0:
            outProcQ(&currentProcess[processorID]->p_list, p);
            break;
        case 1:
            outProcQ(&readyQueue, p);
            break;
        case 2:
            outBlocked(p);
            break;
        default:
            break;
    }
}

void terminateProcess(state_t *statep) {
    pcb_t* toTerminate = NULL;
    int pid = statep->gpr[25];
    int processorID = getPRID();

    if (!globalLock) {
        ACQUIRE_LOCK(&globalLock);
    }

    if (pid == 0) {
        toTerminate = currentProcess[processorID];
        recursiveKiller(toTerminate, 0);
    } else {
        // ricerca del pcb nella currentProcess, nella readyQueue o che aspetta bloccato su un semaforo
        // CURRENTPROCESS
        for (int i = 0; i < NCPU; i++) {
            if (currentProcess[i]->p_pid == pid) {
                toTerminate = currentProcess[i];
                recursiveKiller(toTerminate, 0);
                break;
            }
        }

        // READYQUEUE
        if (toTerminate == NULL) {
            pcb_t* iter = NULL;
            list_for_each_entry(iter, &readyQueue, p_list) {
                if (iter->p_pid == pid) {
                    toTerminate = iter;
                    break;
                }
            }
            recursiveKiller(toTerminate, 1);
        }

        // BLOCKED ON A SEMAPHORE
        if (toTerminate == NULL) {
            toTerminate = outBlockedPid(pid);
            recursiveKiller(toTerminate, 2);
            // TODO: RICORDATI DI GESTIRE IL RITORNO DALLA SYSTEMCALL
            // PCB TROVATOOOOOOOO
        }
    }
    statep->pc_epc += WS;
    LDST(statep);
    RELEASE_LOCK(&globalLock);
}

void systemcallBlock(state_t *statep, int ppid) {
    statep->pc_epc += WS;
    // pacco non credo sia giusto aiuto, qualcuno mi aiuti vi prego non ce la faccio più

    // DIO CANE, PORCO, NON POSSO COPIARLO CON UNA LINEA DI CODICE E MI TOCCA FARE TUTTA QUESTA ROBA A MANO MANNAGGIA A DIO
    currentProcess[ppid]->p_s.cause = statep->cause;
    currentProcess[ppid]->p_s.entry_hi = statep->entry_hi;
    currentProcess[ppid]->p_s.mie = statep->mie;
    currentProcess[ppid]->p_s.pc_epc = statep->pc_epc;
    currentProcess[ppid]->p_s.status = statep->status;
    for (int i = 0; i < STATE_GPR_LEN; i++) {
        currentProcess[ppid]->p_s.gpr[i] = statep->gpr[i];
    }

    // TODO: GESTIONE DEL CAMPO p_time
    ACQUIRE_LOCK(&globalLock);
    currentProcess[ppid] = NULL;
    RELEASE_LOCK(&globalLock);
    scheduler();
}

void passeren(state_t *statep) {
    /*
     * se semAdd = 1 allora decrementa il semaforo e fai ripartire il pcb bloccato su quel semaforo
     * in caso contrario devi fermare il processo su quel semaforo in qualche modo
     */
    // il puntatore in sè equivale alla chiave del semaforo, il contenuto della cella è il valore effettivo del semaforo
    int semadd = statep->gpr[25];
    if (semadd == 0) {
        ACQUIRE_LOCK(&globalLock);
        insertBlocked(&semadd, currentProcess[getPRID()]);
        // il release_lock va messo prima
        RELEASE_LOCK(&globalLock);
        systemcallBlock(statep, getPRID());
    } else {
        ACQUIRE_LOCK(&globalLock);
        semadd--;
        pcb_t *unblocked = removeBlocked(&semadd);
        if (unblocked != NULL) {
            insertProcQ(&readyQueue, unblocked);
        }
        statep->pc_epc += WS;
        LDST(statep);
        RELEASE_LOCK(&globalLock);
    }
}

void verhogen(state_t *statep) {
    int semadd = statep->gpr[25];

    if (semadd == 0) {
        ACQUIRE_LOCK(&globalLock);
        semadd++;
        pcb_t *unblocked = removeBlocked(&semadd);
        if (unblocked != NULL) {
            insertProcQ(&readyQueue, unblocked);
            semadd--;
        }
        statep->pc_epc += WS;
        LDST(statep);
        RELEASE_LOCK(&globalLock);
    } else { // il processo si deve bloccare!
        ACQUIRE_LOCK(&globalLock);
        insertBlocked(&semadd, currentProcess[getPRID()]);
        RELEASE_LOCK(&globalLock);
        systemcallBlock(statep, getPRID());
    }
}

// not so sure di queste, da rivedere

// currentTime dovrebbe contenere la quantità di tempo in microsecondi a partire dal boot del sistema
void getCPUTime(state_t *statep) {
    int ppid = getPRID();
    cpu_t currentTime;
    STCK(currentTime);
    statep->gpr[24] = currentTime + currentProcess[ppid]->p_time;
}

// il 49esimo device equivale allo pseudo clock, per accedervi utilizza la posizione 48
void waitForClock(state_t *statep) {}


void getSupportData(state_t *statep) {
    ACQUIRE_LOCK(&globalLock);
    pcb_t *currentProc = currentProcess[getPRID()];
    statep->gpr[24] = (memaddr)currentProc->p_supportStruct;
    statep->pc_epc += WS;
    LDST(statep);
    RELEASE_LOCK(&globalLock);
}

void getProcessID(state_t *statep) {
    ACQUIRE_LOCK(&globalLock);
    int parent = statep->gpr[25];
    pcb_t *currentProc = currentProcess[getPRID()];
    if (parent == 0) {
        statep->gpr[24] = currentProc->p_pid;
    } else {
        statep->gpr[24] = currentProc->p_parent->p_pid;
    }
    statep->pc_epc += WS;
    LDST(statep);
    RELEASE_LOCK(&globalLock);
}

