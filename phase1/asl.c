#include "./headers/asl.h"
#include "headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

semd_t *findSemaphore(int *semAdd) {    //i decided to go for this one, could also make a RemoveSemaphore

    semd_t *this_sem;

    list_for_each_entry(this_sem, &semd_h, s_link) {

        if (this_sem->s_key == semAdd) {
            return this_sem; // return semaphore with semAdd key
        }
    }
    return NULL; // semaphore not found
}


void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    //praticamente la stessa cosa di initPcbs()
    for (int i = 0; i < MAXPROC; i++) {                     
        INIT_LIST_HEAD(&semd_table[i].s_procq);  
        list_add_tail(&semd_table[i].s_link, &semdFree_h);  
    }
    INIT_LIST_HEAD(&semd_h);
}

/*
 *  setting the semaphore address of the pcb equal to semAdd, since that's
 *  the semaphore were the pcb is blocked, then adding the pcb to the tail
 *  of the process queue associated with the semaphore.
 */
int insertBlocked(int* semAdd, pcb_t* p) {
    semd_t *actualSem = findSemaphore(semAdd);

    if (actualSem) {
        // Semaphore exists, add PCB to its queue
        p->p_semAdd = semAdd;
        list_add_tail(&p->p_list, &actualSem->s_procq);
        return FALSE;
    }

    // Semaphore does not exist so create a new one
    if (list_empty(&semdFree_h)) {
        return TRUE; // No free semaphores available
    }

    // Allocate a semaphore from the free list
    struct list_head *free_entry = semdFree_h.next;
    list_del(free_entry);
    semd_t *newSem = container_of(free_entry, semd_t, s_link);

    // Initialize the new semaphore
    list_add_tail(&newSem->s_link, &semd_h);
    newSem->s_key = semAdd;
    mkEmptyProcQ(&newSem->s_procq);

    // Add the PCB to the new queue of semaphore
    p->p_semAdd = semAdd;
    list_add_tail(&p->p_list, &newSem->s_procq);

    return FALSE;
}

//perdona il miscuglio di engl ed ita ma facevo copia e incolla dalle spec
pcb_t* removeBlocked(int* semAdd) {

    semd_t *this_sem = findSemaphore(semAdd);

    if (this_sem->s_key != semAdd) {                            // se il semaforo non è trovato, restituisce NULL
        return NULL;
    }

    struct list_head *pointer_to_pcb = this_sem->s_procq.next;    
    
    list_del(pointer_to_pcb);                                           // rimuove il nodo dalla lista

    pcb_t *removedPcb = container_of(pointer_to_pcb, pcb_t, p_list);

    
    if (list_empty(&this_sem->s_procq)) {  
                                                            
        list_del(&this_sem->s_link);                        // Se la procq del semaforo è vuota ( cioè abbiamo eliminato l'ultimo pcb)
                                                            // remove the semaphore descriptor from the ASL
        list_add(&this_sem->s_link, &semdFree_h);           // and return it to the semdFree list.
    }

    removedPcb->p_semAdd = NULL;                           //"cancella" il puntatore al semaforo in cui era precedentemente
    return removedPcb;
}

pcb_t* outBlockedPid(int pid) {
    return NULL;
}

pcb_t* outBlocked(pcb_t* p) {
    if (p == NULL || p->p_semAdd == NULL) {
        return NULL;
    }

    semd_t *this_sem = findSemaphore(p->p_semAdd);

    if(this_sem==NULL){
        return NULL;
    }
                                                        
    struct list_head *pos;                         // if the semaphore is found we search for p in its procq
    list_for_each(pos, &this_sem->s_procq) {

        pcb_t *pbc_pointed_to_by_p = container_of(pos, pcb_t, p_list);

        if (pbc_pointed_to_by_p == p) {            // deletes the pbc from the queue and changes it's semAdd
                                                        
            list_del(&p->p_list);
            p->p_semAdd = NULL;
  
            if (emptyProcQ(&this_sem->s_procq)) {  //exact same thing as last functions, deletes a semaphore if emtpy

                list_del(&this_sem->s_link);

                list_add_tail(&this_sem->s_link, &semdFree_h);
            }

            return p; 
        }
    }
            
    return NULL;                    // if the pcb isn't in the queue return NULL
}


pcb_t* headBlocked(int* semAdd) {

    semd_t *this_sem = findSemaphore(semAdd);
    
    if (!this_sem) {
        
        return NULL;   
    }

    if (list_empty(&this_sem->s_procq)) {
        
        return NULL;                    //if the procq is empty then there's no pcbs to look for, although the function doesn't say
                                        //to eliminate the semaphore from the ASL like the others
    }
    struct list_head *pointer_to_pcb = this_sem->s_procq.next;           // Return the PCB at the head of the process queue
    return container_of(pointer_to_pcb, pcb_t, p_list);
}


    

