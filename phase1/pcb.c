#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {

    // initialize the list as empty
    INIT_LIST_HEAD(&pcbFree_h);                                 
    for (int i = 0; i < MAXPROC; i++) {
        // add pcb to the tail of the list
        list_add_tail(&pcbFree_table[i].p_list, &pcbFree_h);    
    }
    
}

void freePcb(pcb_t* p) {

    // insert a free pcb at the tail of the pcbFree list
    if (p != NULL) {
        list_add_tail(&p->p_list, &pcbFree_h); 
    }

}

pcb_t* allocPcb() {

    // find out if there's free pcbs
    if (list_empty(&pcbFree_h)) {
        return NULL;                           
    }

    // deletes the first pcb from the list
    struct list_head* first_pcb = pcbFree_h.next;
    list_del(first_pcb);                       

    // obtain the corrisponding pcb
    pcb_t* pcb = container_of(first_pcb, pcb_t, p_list); 

    // initialize all the fields of the pcb
    pcb->p_parent = NULL;                      
    INIT_LIST_HEAD(&pcb->p_child); 
    INIT_LIST_HEAD(&pcb->p_sib); 
    pcb->p_supportStruct = NULL; 
    pcb->p_semAdd = NULL;     
    pcb->p_pid = next_pid;
    next_pid++;      
    pcb->p_time = 0;   

    return pcb;
}

void mkEmptyProcQ(struct list_head* head) {

    INIT_LIST_HEAD(head);   

}

int emptyProcQ(struct list_head* head) {

    return list_empty(head);

}

void insertProcQ(struct list_head* head, pcb_t* p) {

    list_add_tail(&p->p_list, head);

}

pcb_t* headProcQ(struct list_head* head) {

    if (emptyProcQ(head)) return NULL;

    return container_of(head->next, pcb_t, p_list);

}

pcb_t* removeProcQ(struct list_head* head) {

    if (emptyProcQ(head)) return NULL;

    struct list_head* p = head->next;
    list_del(head->next);

    return container_of(p, pcb_t, p_list);

}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {

    int flag=0;
    struct list_head* iter;

    list_for_each(iter, head) {
        if(iter == &p->p_list){
            flag = 1;
        }
    }

    if (flag) { 
        list_del(&p->p_list);
        return p; 
        } 

    else return NULL;

}

int emptyChild(pcb_t* p) {

    return list_empty(&p->p_child);

}


void insertChild(pcb_t* prnt, pcb_t* p) {

    if (emptyChild(prnt)) {
        INIT_LIST_HEAD(&prnt->p_child);
    }

    p->p_parent = prnt;
    list_add_tail(&p->p_sib, &prnt->p_child);
}

pcb_t* removeChild(pcb_t* p) {

    struct list_head *first_child = p->p_child.next;

    if (list_empty(&p->p_child)) {
        return NULL;
    } 

    else {
        list_del(first_child);
        return container_of(first_child, pcb_t, p_list);
    }

}

pcb_t* outChild(pcb_t* p) {

    if (p->p_parent == NULL) {
        return NULL;
    }

    list_del(&p->p_sib);
    p->p_parent = NULL;

    return p;

}
