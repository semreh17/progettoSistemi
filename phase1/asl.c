#include "./headers/asl.h"
#include "headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

/*
 * auxiliary function to search for a semaphore inside the ASL,
 * using the s_key attribute of the semd_t structure
 */
semd_t *findSemaphore(int *semAdd) {

    semd_t *this_sem;

    list_for_each_entry(this_sem, &semd_h, s_link) {

        if (this_sem->s_key == semAdd) {
            return this_sem; // return semaphore with semAdd key
        }
    }
    return NULL; // semaphore not found
}

/*
 * initialization of the semdFree list
 */
void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
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

    if (list_empty(&semdFree_h)) {
        return TRUE; // No free semaphores available
    }

    // Allocate a semaphore from the free list
    struct list_head *free_entry = semdFree_h.next;
    list_del(free_entry);
    semd_t *newSem = container_of(free_entry, semd_t, s_link);

    // Initialization of the new semaphore
    list_add_tail(&newSem->s_link, &semd_h);
    newSem->s_key = semAdd;
    mkEmptyProcQ(&newSem->s_procq);

    p->p_semAdd = semAdd;
    list_add_tail(&p->p_list, &newSem->s_procq);

    return FALSE;
}

/*
 * removing a PCB from the process queue of the semaphore with
 * semAdd key that identifies the semaphore
 */
pcb_t* removeBlocked(int* semAdd) {

    semd_t *this_sem = findSemaphore(semAdd);
    // if the semaphore is not found return NULL
    if (this_sem->s_key != semAdd) {
        return NULL;
    }

    struct list_head *pointer_to_pcb = this_sem->s_procq.next;
    // removing the node from the list
    list_del(pointer_to_pcb);
    pcb_t *removedPcb = container_of(pointer_to_pcb, pcb_t, p_list);

    // if the process queue of the semaphore is empty, remove
    // the semaphore from the ASL
    if (list_empty(&this_sem->s_procq)) {
        list_del(&this_sem->s_link);
        list_add(&this_sem->s_link, &semdFree_h);
    }
    removedPcb->p_semAdd = NULL;
    return removedPcb;
}

// in realtà cerca il processo bloccato nel semaforo e lo restituisce
pcb_t* outBlockedPid(int pid) {
    semd_t* semaphoreIter = NULL;
    pcb_t* blockedProcIter = NULL;
    list_for_each_entry(semaphoreIter, &semd_h, s_link) {
        list_for_each_entry(blockedProcIter, &semaphoreIter->s_procq, p_list) {
            if (blockedProcIter->p_pid == pid) {
                return blockedProcIter;
            }
        }
    }
    return NULL;
}

/*
 * removing the PCB, pointed to by p, from the semaphore process queue
 * where the PCB is blocked
 */
pcb_t* outBlocked(pcb_t* p) {
    if (p == NULL || p->p_semAdd == NULL) {
        return NULL;
    }

    semd_t *this_sem = findSemaphore(p->p_semAdd);

    if(this_sem==NULL){
        return NULL;
    }

    // if the semaphore is found we search for p in its process queue
    struct list_head *pos;
    list_for_each(pos, &this_sem->s_procq) {
        pcb_t *pbc_pointed_to_by_p = container_of(pos, pcb_t, p_list);

        // deletes the pbc from the queue and changes it's semAdd
        if (pbc_pointed_to_by_p == p) {
            list_del(&p->p_list);
            p->p_semAdd = NULL;

            //exact same thing as last functions, deletes a semaphore if emtpy
            if (emptyProcQ(&this_sem->s_procq)) {
                list_del(&this_sem->s_link);
                list_add_tail(&this_sem->s_link, &semdFree_h);
            }
            return p; 
        }
    }
    // if the pcb isn't in the queue return NULL
    return NULL;
}

pcb_t* headBlocked(int* semAdd) {
    semd_t *this_sem = findSemaphore(semAdd);

    // semAdd not found on the ASL
    if (!this_sem) {
        return NULL;   
    }
    //if the procq is empty then there's no pcbs to look for, although the function
    //doesn't say to eliminate the semaphore from the ASL like the others
    if (list_empty(&this_sem->s_procq)) {
        return NULL;
    }
    // Return the PCB at the head of the process queue
    struct list_head *pointer_to_pcb = this_sem->s_procq.next;
    return container_of(pointer_to_pcb, pcb_t, p_list);
}