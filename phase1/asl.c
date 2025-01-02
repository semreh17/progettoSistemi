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
    // semd_t actualSem = semd_table[*semAdd]; //semd_table è array statico di grandezza maxproc, non capisco come potrebbe funzionare
    semd_t actualSem;
    // searching for the semaphore, with semAdd key, inside the array of semaphores
    for (int i = 0; i < MAXPROC; i++) {
        if (*semd_table[i].s_key == *semAdd) { // not sure if "*" operator is needed, but i actually need to check the value inside the memory cell
            actualSem = semd_table[i];
        }
    }
    if (emptyProcQ(&actualSem.s_procq)) {   //mettere come indice *semAdd che probabilmente è una cella di memoria tipo Ax542rD_some_shit_65RtF$
        if (list_empty(&semdFree_h)) {
            return TRUE;
        }
        // okay maybe the contaierOf is not necessary
        semd_t *newSem = container_of(&semdFree_h, semd_t, s_link);  //perchè container_of? il newSem è un elemento della lista semdFree_h
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

//perdona il miscuglio di engl ed ita ma facevo copia e incolla dalle spec
pcb_t* removeBlocked(int* semAdd) {
    semd_t *this_sem;

    list_for_each_entry(this_sem, &semd_h, s_link) {           // search the ASL for a descriptor of this semaphore.
        if (this_sem->s_key == semAdd) {                       // questo pezzo serve per cercare un semaforo puntato da semAdd
            break;                                             // nella ASL, fate copia ed incolla se e quando servirà per altro codice 
        }
    }
    
    if (this_sem->s_key != semAdd) {                            // se il semaforo non è trovato, restituisce NULL
        return NULL;
    }

    struct list_head *first = this_sem->s_procq.next;    
    
    list_del(first);                                           // rimuove il nodo dalla lista

    pcb_t *removedPcb = container_of(first, pcb_t, p_list);

    
    if (list_empty(&this_sem->s_procq)) {  
                                                            
        list_del(&this_sem->s_link);                        // Se la porcq del semaforo è vuota ( cioè abbiamo eliminato l'ultimo pcb)
                                                            // remove the semaphore descriptor from the ASL
        list_add(&this_sem->s_link, &semdFree_h);           // and return it to the semdFree list.
    }

    removedPcb->p_semAdd = NULL;                           //"cancella" il puntatore al semaforo in cui era precedentemente
    return removedPcb;
}

pcb_t* outBlockedPid(int pid) {
}

pcb_t* outBlocked(pcb_t* p) {
    list_for_each_entry() {

    }
}

pcb_t* headBlocked(int* semAdd) {
}
