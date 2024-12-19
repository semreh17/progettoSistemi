#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
    INIT_LIST_HEAD(&pcbFree_h); // Inizializza la lista come vuota
    for (int i = 0; i < MAXPROC; i++) {
        list_add_tail(&pcbFree_table[i].p_list, &pcbFree_h); // Aggiunge i PCB nella coda della lista
    }
}

void freePcb(pcb_t* p) {
    if (p != NULL) {
        list_add_tail(&p->p_list, &pcbFree_h); // Inserisce in coda alla lista dei processi liberi
    }
}

pcb_t* allocPcb() {
    if (list_empty(&pcbFree_h)) {
        return NULL; // controlla che ci siano dei PCBs dispondibili
    }

    
    struct list_head* first_pcb = pcbFree_h.next;
    list_del(first_pcb); // prende e rimuove dalla lista il primo elemento

    pcb_t* pcb = container_of(first_pcb, pcb_t, p_list); // Ottieni il PCB corrispondente
    
    
        //inizializzazione di tutti i campi a "NULL"
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

    INIT_LIST_HEAD(&head);             //head->next/previous= head
    
}

int emptyProcQ(struct list_head* head) {
    return list_empty(head);
}

void insertProcQ(struct list_head* head, pcb_t* p) {
    list_add_tail(p, head);
}

pcb_t* headProcQ(struct list_head* head) {
    if (emptyProcQ(head)) return NULL;
    return container_of(head->next, pcb_t, p_list);
}

pcb_t* removeProcQ(struct list_head* head) {
    if (emptyProcQ(head)) return NULL;
    struct list_head* p = head->next;
    list_del(head->next);
    // devo passargli il pcb? Oppure il list_head che lo punta, boh
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
        } else return NULL;
}

int emptyChild(pcb_t* p) {

    if (list_empty(&p)) {
        return TRUE;                    //se la lista è vuota return TRUE
    }
    else return FALSE;   
}

void insertChild(pcb_t* prnt, pcb_t* p) {

    list_add_tail(&p->p_list, &prnt->p_child);         //aggiunge p alla lista p_child di prnt

}

pcb_t* removeChild(pcb_t* p) {
    struct list_head *first_child=p->p_child.next;
     if (list_empty(&p->p_child)) {
        return NULL;                    //se la lista dei children è vuota return NULL
    } else {
        list_del(&first_child);         //elimina
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
