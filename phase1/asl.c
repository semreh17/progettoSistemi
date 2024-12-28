#include "./headers/asl.h"

#include "headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

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
    semd_t actualSem = semd_table[*semAdd];
    if (emptyProcQ(&actualSem.s_procq)) {
        if (list_empty(&semdFree_h)) {
            return TRUE;
        }
        
        semd_t *newSem = container_of(&semdFree_h, semd_t, s_link);
        list_del(semdFree_h.next);

        // passing the address of the list, so the new sem can be added to the tail
        // then initializing the fields
        list_add_tail(&newSem->s_link, &semd_h);
        newSem->s_key = semAdd;
        mkEmptyProcQ(&newSem->s_procq);

        p->p_semAdd = semAdd;
        list_add_tail(&p->p_list, &newSem->s_procq);
    }
    return FALSE;
}

pcb_t* removeBlocked(int* semAdd) {

}

pcb_t* outBlockedPid(int pid) {
}

pcb_t* outBlocked(pcb_t* p) {
}

pcb_t* headBlocked(int* semAdd) {
}
