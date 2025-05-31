#include "../phase1/headers/pcb.h"
extern int processCount;         // quanti processi ci sono in giro
extern pcb_t *currentProcess[NCPU];  // Chi sta girando su ogni core
extern struct list_head readyQueue;  // La fila dei processi pronti
extern struct list_head semd_h;
extern int deviceSemaphores[NRSEMAPHORES];
extern volatile unsigned int globalLock;  // Lock globale per la sincronizzazione
#define INCURRENTPROCESS 0
#define ONREADYQUEUE 1
#define BLOCKEDONASEM 2

extern void interruptHandler(int cause, state_t *statep);

extern void klog_print();
extern void klog_print_dec();
extern void klog_print_hex();

void *memcpy(void *dest, const void *src, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++)
    {
        ((char*)dest)[i] = ((char*)src)[i];
    }
}

// questa probably è rotta

void recursiveKiller(pcb_t *p, unsigned int location) {
    klog_print(" ADESSO DO DI MATTO CAZZO ");

    if (p == NULL) {
        klog_print(" processo mancante? ");
        return;
    }

    // Rendo orfani i figli con outChild e poi chiamata ricorsiva
    while (!emptyChild(p)) {
        pcb_t *child = removeChild(p);
        outChild(child);
        recursiveKiller(child, location);
    }

    unsigned int processorID = getPRID();

    switch (location) {
        case INCURRENTPROCESS:
            klog_print(" processore corrente ");
            // TODO: not so sure di questa, accertarsi se effettivamente si debba fare la outProcQ sul quella lista
            outProcQ(&currentProcess[processorID]->p_list, p);
            currentProcess[processorID] = NULL;
            break;
        case ONREADYQUEUE:
            klog_print(" ready queue ");
            outProcQ(&readyQueue, p);
            break;
        case BLOCKEDONASEM:
            klog_print(" su un mrda di semaforo");
            // mi salvo il semaforo e dopo aver tolto il processo bloccato, incremento il semaforo
            int *semAddr = p->p_semAdd;
            outBlocked(p);
            (*semAddr)++;
            break;
        default:
            break;
    }

    freePcb(p);
    klog_print(" xxxxx ");
    klog_print_dec(p->p_pid);
    processCount--;
}

// scheduler riparte non trova nessun processo da schedulare e rientra di nuovo in passUpOrDie e il ciclo si ripete
void passUpOrDie(state_t *statep, int exceptIndex) {
    ACQUIRE_LOCK(&globalLock);
    int ppid = getPRID();

    if (currentProcess[ppid] == NULL) {
        RELEASE_LOCK(&globalLock);
        scheduler();
    }

    if (currentProcess[ppid]->p_supportStruct == NULL) {
        klog_print_dec(processCount);
        // qui chiama recursiveKiller ma in currentProcess non c'è nessun processo attivo
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
    klog_print(" ziocan ");
    scheduler();
}

void terminateProcess(state_t *statep) {
    pcb_t* toTerminate = NULL;
    int pid = statep->gpr[25];
    int processorID = getPRID();
    ACQUIRE_LOCK(&globalLock);

    if (pid == 0) {
        toTerminate = currentProcess[processorID];
        recursiveKiller(toTerminate, INCURRENTPROCESS);
        RELEASE_LOCK(&globalLock);
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


            *toTerminate->p_semAdd++;

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
    klog_print(" ciao mi sto per bloccare ");
    klog_print_dec(getPRID());
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
    klog_print(" ciaomiao ");
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
    if (currentProcess[0] != NULL) klog_print(" PROCESSO NON VUOTO  AAAAAAA");

    int core_id = getPRID();  //funzione strana di uriscv per prendere l'ID del core
    state_t *exc_state = GET_EXCEPTION_STATE_PTR(core_id);    // Prendiamo lo stato del processo che ha generato l'eccezione
    unsigned int cause = getCAUSE(); //funzione di uriscv per prendere la causa dell'eccezione
    int exc_code = cause & CAUSE_EXCCODE_MASK; //CAUSE_EXCCODE_MASK è una maschera per prendere i bit 0-5 della causa dell'eccezione

    if (CAUSE_IS_INT(getCAUSE())) {    //controlla solo il bit più significativo (bit 31), se 1 interrupt se 0 eccezione
        interruptHandler(exc_code, exc_state);
    } else {
        if (exc_code >= 24 && exc_code <= 28) {
            klog_print("caso da 24 a 28");
            passUpOrDie(exc_state, PGFAULTEXCEPT);
        } else if (exc_code == 8 || exc_code == 11) {
            // controllo delle systemCall con numeri maggiori di 0
            int syscall_num = exc_state->reg_a0;
            if (syscall_num > 0) {
                klog_print("NUMERO SYSCALL POSITIVO");
                passUpOrDie(exc_state, GENERALEXCEPT);
            }

            //TODO: CONTROLLARE SE IL PROCESSO È IN USER O KERNEL MODE

            switch (syscall_num) {
                case CREATEPROCESS: {  // -1
                    createProcess(exc_state);
                    break;
                }
                case TERMPROCESS:    // -2
                    klog_print(" il dio bestia ");
                    terminateProcess(exc_state);
                    break;

                case PASSEREN:       // -3
                    passeren(exc_state);
                    break;

                case VERHOGEN:       // -4
                    verhogen(exc_state);
                    break;

                case DOIO:           // -5
                    doIO(exc_state);
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
                    PANIC();  // Syscall non riconosciuta
                    return;
            }
        } else if ((exc_code >= 0 && exc_code <= 7) || exc_code == 9 || exc_code == 10 || (exc_code >= 12 && exc_code <= 23)) {
            passUpOrDie(exc_state, GENERALEXCEPT); // passUpOrDie for general exception
        }
    }
}
