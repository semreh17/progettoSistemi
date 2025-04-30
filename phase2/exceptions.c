#include "../phase1/headers/pcb.h"
extern int processCount;         // quanti processi ci sono in giro
extern pcb_t *currentProcess[NCPU];  // Chi sta girando su ogni core
extern struct list_head readyQueue;  // La fila dei processi pronti
extern struct list_head semd_h;


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
        outChild(toTerminate);
        recursiveKiller(toTerminate, 0);
    } else {
        // ricerca del pcb nella currentProcess, nella readyQueue o che aspetta bloccato su un semaforo
        // CURRENTPROCESS
        for (int i = 0; i < NCPU; i++) {
            if (currentProcess[i]->p_pid == pid) {
                toTerminate = currentProcess[i];
                outChild(toTerminate);
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
            outChild(toTerminate);
            recursiveKiller(toTerminate, 1);
        }

        // BLOCKED ON A SEMAPHORE
        if (toTerminate == NULL) {
            semd_t* semaphoreIter = NULL;
            pcb_t* blockedProcIter = NULL;
            int foundFlag = 0;
            // CICLARE SU TUTTI I SEMAFORI ATTIVI, PER OGNI SEMAFORO CERCARE NELLA LISTA DI PCB BLOCCATI
            list_for_each_entry(semaphoreIter, &semd_h, s_link) {
                list_for_each_entry(blockedProcIter, &semaphoreIter->s_procq, p_list) {
                    if (blockedProcIter->p_pid == pid) {
                        toTerminate = blockedProcIter;
                        foundFlag = 1;
                        break;
                    }
                }
                if (foundFlag) {
                    outChild(toTerminate);
                    recursiveKiller(toTerminate, 2);
                    break;
                }
            }
        }
        // TODO: RICORDATI DI GESTIRE IL RITORNO DALLA SYSTEMCALL
        // PCB TROVATOOOOOOOO
    }
}

void systemcallBlock(state_t *statep) {
    statep->pc_epc += WS;
    currentProcess[getPRID()]->p_s = *statep;
    // TODO: GESTIONE DEL CAMPO p_time
    scheduler();
}

void passeren(state_t *statep) {
    /*
     * se semAdd = 1 allora decrementa il semaforo e fai ripartire il pcb bloccato su quel semaforo
     * in caso contrario devi fermare il processo su quel semaforo in qualche modo
     */
    // il puntatore in sè equivale alla chiave del semaforo, il contenuto della cella è il valore effettivo del semaforo
    int *semadd = statep->gpr[25];
    if (*semadd == 0) {
        ACQUIRE_LOCK(&globalLock);
        insertBlocked(semadd, currentProcess[getPRID()]);
        // il release_lock va messo prima
        RELEASE_LOCK(&globalLock);
        systemcallBlock(statep);
    } else {
        ACQUIRE_LOCK(&globalLock);
        *semadd--;
        pcb_t *unblocked = removeBlocked(semadd);
        if (unblocked != NULL) {
            insertProcQ(&readyQueue, unblocked);
        }
        // qui forse manca qualcosa (?)
        statep->pc_epc += WS;
        RELEASE_LOCK(&globalLock);
    }
}

void verhogen(state_t *statep) {
    int *semadd = statep->gpr[25];

    if (*semadd == 0) {
        ACQUIRE_LOCK(&globalLock);
        *semadd++;
        pcb_t *unblocked = removeBlocked(semadd);
        if (unblocked != NULL) {
            insertProcQ(&readyQueue, unblocked);
            semadd--;
        }
        // qui forse manca qualcosa (?)
        statep->pc_epc += WS;
        RELEASE_LOCK(&globalLock);
    } else { // il processo si deve bloccare!
        ACQUIRE_LOCK(&globalLock);
        insertBlocked(semadd, currentProcess[getPRID()]);
        RELEASE_LOCK(&globalLock);
        systemcallBlock(statep);
    }
}