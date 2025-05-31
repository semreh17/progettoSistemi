extern int processCount;
extern volatile unsigned int globalLock;
extern struct list_head readyQueue;
extern pcb_t *currentProcess[NCPU];

void scheduler() {
    unsigned int coreId = getPRID();
    ACQUIRE_LOCK(&globalLock);
    if (!emptyProcQ(&readyQueue)) {
        pcb_t *newProcess = removeProcQ(&readyQueue);  //rimuove processo dalla queue
        currentProcess[coreId] = newProcess;      //assegnazione al core corrente
        *((memaddr *)TPR) = 0; // setting the TPR to 0
        RELEASE_LOCK(&globalLock);

        setTIMER(TIMESLICE * (*(cpu_t *)TIMESCALEADDR));
        LDST(&newProcess->p_s);  //caricamento process state
    } else {
        if (processCount == 0) {     //se la coda è vuota
            RELEASE_LOCK(&globalLock);
            HALT();
        } else {
            *((memaddr *)TPR) = 1; // setting the TPR to 1
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            RELEASE_LOCK(&globalLock);
            setSTATUS(status);  // disabilita timer interrupt


            WAIT();
        }
    }
}