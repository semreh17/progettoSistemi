#include "../phase1/headers/pcb.h"
#include <stddef.h>
extern int processCount;         // quanti processi ci sono in giro
extern pcb_t *currentProcess[NCPU];  // Chi sta girando su ogni core
extern struct list_head readyQueue;  // La fila dei processi pronti
extern struct list_head semd_h;
extern int deviceSemaphores[NRSEMAPHORES];

#define INCURRENTPROCESS 0
#define ONREADYQUEUE 1
#define BLOCKEDONASEM 2

extern void klog_print();

void *memcpy(void *dest, const void *src, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++)
    {
        ((char*)dest)[i] = ((char*)src)[i];
    }
}

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


void passUpOrDie(state_t *statep, int exceptIndex) {
    ACQUIRE_LOCK(&globalLock);
    int ppid = getPRID();
    if (currentProcess[ppid]->p_supportStruct == NULL) {
        recursiveKiller(currentProcess[ppid], INCURRENTPROCESS);
    } else {
        // c'è da salvare lo stato nel processo corrente!
        // TODO: CAMBIARE IL MEMCPY CON L'UGUALE
        memcpy(&currentProcess[ppid]->p_supportStruct->sup_exceptState[exceptIndex], statep, sizeof(state_t));
        LDCXT(currentProcess[ppid]->p_supportStruct->sup_exceptContext[exceptIndex].stackPtr,
              currentProcess[ppid]->p_supportStruct->sup_exceptContext[exceptIndex].status,
              currentProcess[ppid]->p_supportStruct->sup_exceptContext[exceptIndex].pc);
    }
    RELEASE_LOCK(&globalLock);
    scheduler();
}

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
            // TODO: CAMBIARE IL MEMCPY CON L'UGUALE
            memcpy(&currentProcess[core_id]->p_s, exc_state, sizeof(state_t));

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
        klog_print("qua si blocca, IL_TIMER? ");
        klog_print_dec(core_id);
        klog_print("\n");
        LDST(&currentProcess[core_id]->p_s);
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
        LDST(&currentProcess[core_id]->p_s);
        return;
    }
    
    RELEASE_LOCK(&globalLock);
    PANIC();
}

void terminateProcess(state_t *statep) {
    pcb_t* toTerminate = NULL;
    int pid = statep->gpr[25];
    int processorID = getPRID();
    ACQUIRE_LOCK(&globalLock);

    if (pid == 0) {
        toTerminate = currentProcess[processorID];
        recursiveKiller(toTerminate, INCURRENTPROCESS);
        scheduler();
    } else {
        // ricerca del pcb nella currentProcess, nella readyQueue o che aspetta bloccato su un semaforo
        // tutta questa parte sarebbe meglio metterla dentro una apposita funzione
        // CURRENTPROCESS
        for (int i = 0; i < NCPU; i++) {
            if (currentProcess[i]->p_pid == pid) {
                toTerminate = currentProcess[i];
                recursiveKiller(toTerminate, INCURRENTPROCESS);
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
            recursiveKiller(toTerminate, ONREADYQUEUE);
        }

        // BLOCKED ON A SEMAPHORE
        if (toTerminate == NULL) {
            toTerminate = outBlockedPid(pid);
            recursiveKiller(toTerminate, BLOCKEDONASEM);
            // PCB TROVATOOOOOOOO
        }
        RELEASE_LOCK(&globalLock);
        scheduler();
    }
    statep->pc_epc += WS;
    LDST(statep);
    RELEASE_LOCK(&globalLock);
}


void systemcallBlock(state_t *statep, int ppid) {
    statep->pc_epc += WS;
    // TODO: CAMBIARE IL MEMCPY CON L'UGUALE
    memcpy(&currentProcess[ppid]->p_s, statep, sizeof(state_t));

    // time updated for current process
    cpu_t currentTime;
    STCK(currentTime);
    currentProcess[ppid]->p_time += currentTime;

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
    int *semadd = (int*)&statep->reg_a1;
    if (*semadd == 0) {
        ACQUIRE_LOCK(&globalLock);
        insertBlocked(semadd, currentProcess[getPRID()]);
        // il release_lock va messo prima
        RELEASE_LOCK(&globalLock);
        systemcallBlock(statep, getPRID());
    } else {
        ACQUIRE_LOCK(&globalLock);
        *semadd--;
        pcb_t *unblocked = removeBlocked(semadd);
        if (unblocked != NULL) {
            insertProcQ(&readyQueue, unblocked);
        }
        statep->pc_epc += WS;
        LDST(statep);
        RELEASE_LOCK(&globalLock);
    }
}

void verhogen(state_t *statep) {
    int *semadd = (int*)&statep->reg_a1;

    if (semadd == 0) {
        ACQUIRE_LOCK(&globalLock);
        *semadd++;
        pcb_t *unblocked = removeBlocked(semadd);
        if (unblocked != NULL) {
            insertProcQ(&readyQueue, unblocked);
            *semadd--;
        }
        statep->pc_epc += WS;
        LDST(statep);
        RELEASE_LOCK(&globalLock);
    } else { // il processo si deve bloccare!
        ACQUIRE_LOCK(&globalLock);
        insertBlocked(semadd, currentProcess[getPRID()]);
        RELEASE_LOCK(&globalLock);
        systemcallBlock(statep, getPRID());
    }
}
// not so sure di queste, da rivedere

void doIO(state_t *statep) {
    // devo cercare il device giusto, ma come mmmmmmmm
    // in teoria mi basta scorrere onguna delle tot linee e per ogni linea
    // confrontare il command field con quello che mi viene passato come argomento

    klog_print(" xxxxxxxxx ");
    unsigned int *commandAdr = (unsigned int*)statep->reg_a1;
    int commandVal = statep->reg_a2;

    // "indice" del device
    int getDev = (int) commandAdr - DEV_REG_START;

    // numero del device
    int devNo = getDev % 0x8;
    // blocco il semaforo con il getprid
    ACQUIRE_LOCK(&globalLock);
    // se è dispari lo blocchi sul device semaphore 0 altrimenti sul numero 1
    
    insertBlocked(&deviceSemaphores[devNo], currentProcess[getPRID()]);
    RELEASE_LOCK(&globalLock);
    *commandAdr = commandVal;
    scheduler();
}

// currentTime dovrebbe contenere la quantità di tempo in microsecondi a partire dal boot del sistema
void getCPUTime(state_t *statep) {
    int ppid = getPRID();
    cpu_t currentTime;
    STCK(currentTime);
    statep->gpr[24] = currentTime + currentProcess[ppid]->p_time;
    statep->pc_epc += WS;
    LDST(statep);
}

// il 49esimo device equivale allo pseudo clock, per accedervi utilizza la posizione 48
// blocca il processo sul semaforo dello pseudo-clock
void waitForClock(state_t *statep) {
    ACQUIRE_LOCK(&globalLock);
    insertBlocked(&deviceSemaphores[48], currentProcess[getPRID()]);
    RELEASE_LOCK(&globalLock);
    systemcallBlock(statep, getPRID());
}


void getSupportData(state_t *statep) {
    ACQUIRE_LOCK(&globalLock);
    pcb_t *currentProc = currentProcess[getPRID()];
    // faccio il cast con memaddr rendendolo un unsigned int
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

void createProcess(state_t *statep) {
    ACQUIRE_LOCK(&globalLock);
    pcb_t *new_pcb = allocPcb();
    if (!new_pcb) {
        statep->reg_a1 = -1;  // Errore: PCB non allocato
        RELEASE_LOCK(&globalLock);
        return;
    }

    // Inizializzazione PCB
    // al campo p_s restituisco il contenuto della cella contenente lo state presente nel registro a1
    new_pcb->p_s = *(state_t*)statep->reg_a1;
    new_pcb->p_supportStruct = (support_t*)statep->reg_a3;
    new_pcb->p_time = 0;
    new_pcb->p_semAdd = NULL;

    // Gestione gerarchia e scheduling
    insertProcQ(&readyQueue, new_pcb);
    insertChild(currentProcess[getPRID()], new_pcb);
    processCount++;

    statep->reg_a1 = new_pcb->p_pid;  // Successo: restituisci PID
    RELEASE_LOCK(&globalLock);
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
        case 0 ... 7:
            passUpOrDie(exc_state, GENERALEXCEPT);
            break;
        case 9 ... 10:
            passUpOrDie(exc_state, GENERALEXCEPT);
            break;
        case 8:  // SYSCALL
        case 11: // SYSCALL
            // handleSyscall(exc_state);  // il gestore di syscall
            // controllo delle systemCall con numeri maggiori di 0
            int syscall_num = exc_state->reg_a0;
            if (syscall_num > 0) {
                passUpOrDie(exc_state, GENERALEXCEPT);
                break;
            }
            switch (syscall_num) {
            case CREATEPROCESS: {  // -1
                createProcess(exc_state);
                break;
            }
            case TERMPROCESS:    // -2
                terminateProcess(exc_state);
                break;

            case PASSEREN:       // -3
                passeren(exc_state);
                break;

            case VERHOGEN:       // -4
                verhogen(exc_state);
                break;

            case DOIO:           // -5
                // Implementa operazione I/O... credo in te Hermes
                break;

            case GETTIME:        // -6
                getCPUTime(exc_state);
                break;

            case CLOCKWAIT:      // -7
                waitForClock(exc_state);
                break;

            case GETSUPPORTPTR:  // -8
                getSupportData(exc_state);
                break;

            case GETPROCESSID:   // -9
                getProcessID(exc_state);
                break;

            default:
                klog_print("ciao qua esco panico");
                PANIC();  // Syscall non riconosciuta
                return;
        }
            break;
        case 12 ... 23:
            passUpOrDie(exc_state, GENERALEXCEPT);
            break;
        case 24 ... 28:
            passUpOrDie(exc_state, PGFAULTEXCEPT);
            break;

        default: // tutto gli altri casi (Program Trap)
            passUpOrDie(exc_state, GENERALEXCEPT);
            break;
    }
}
