#include "./headers/asl.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initASL() {
    INIT_LIST_HEAD(&semdFree_h);

    
    for (int i = 0; i < MAXPROC; i++) {              //praticamente la stessa cosa di initPcbs()
        INIT_LIST_HEAD(&semd_table[i].s_procq);  
        list_add_tail(&semd_table[i].s_link, &semdFree_h);  
    }

    INIT_LIST_HEAD(&semd_h);
}

int insertBlocked(int* semAdd, pcb_t* p) {
    
}

pcb_t* removeBlocked(int* semAdd) {
}

pcb_t* outBlockedPid(int pid) {
}

pcb_t* outBlocked(pcb_t* p) {
}

pcb_t* headBlocked(int* semAdd) {
}
