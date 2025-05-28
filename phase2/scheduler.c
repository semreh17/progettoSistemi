extern int processCount;
extern volatile unsigned int globalLock;
extern struct list_head readyQueue;
extern pcb_t *currentProcess[NCPU];
extern void klog_print();
void scheduler() {
    
  /* Remove the PCB from the head of the Ready Queue and store the pointer to the PCB in the
Current Process field of the current CPU.
*/
  ACQUIRE_LOCK(&globalLock);
  // Controllo se la ready queue è vuota
  if (emptyProcQ(&readyQueue)) {
    if (processCount == 0) { 
      // Ready queue vuota e nessun processo in esecuzione, quindi HALT
      RELEASE_LOCK(&globalLock); // Rilascio del lock prima di fermare l'esecuzione, quindi HALT
      HALT(); // Fermiamo l'esecuzione
    } else {
      // Ready queue vuota, ma ci sono processi in esecuzione, quindi WAIT
      setMIE(MIE_ALL & ~MIE_MTIE_MASK);
      unsigned int status = getSTATUS();
      status |= MSTATUS_MIE_MASK;
      *((memaddr * ) TPR) = 1; // // Impostiamo TPR a 1 prima di WAIT
      RELEASE_LOCK(&globalLock); // Rilascio del lock prima di WAIT
      setSTATUS(status);
      WAIT();    // Entra in Wait State
    }
  } else {
    // La ready queue non è vuota: selezioniamo e dispatchiamo il prossimo processo
    int pid = getPRID();
    pcb_t *pcb = removeProcQ(&readyQueue);     // Rimuoviamo il processo dalla Ready Queue
    currentProcess[pid] = pcb;              // Aggiorniamo il current process del CPU
     // Carichiamo il valore di time slice nel PLT per il processo appena dispatchato
    *((memaddr*)TPR) = 0; // Settiamo il TPR a 0 per abilitare le interruzioni
   // STCK(start_time[pid]);  // settiamo il tempo di inizio del processo
      
    // Rilasciamo il lock dopo aver completato il dispatch
    RELEASE_LOCK(&globalLock);
    setTIMER(TIMESLICE * (*(cpu_t *)TIMESCALEADDR));
    // Carichiamo il contesto del processo
    LDST(&(pcb->p_s));
  }
}
