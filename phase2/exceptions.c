#include "../headers/types.h"
#include "../headers/listx.h"
#include "../headers/const.h"
#include "../phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"

void exceptionHandler() {}

int createProcess(state_t *statep, pcb_t *caller) {
    pcb_t *newProcess = allocPcb();
    if (newProcess == NULL) {
        statep->gpr[24] = -1;
        return NULL;
    }

    newProcess->p_s = * (state_t*)statep->gpr[25];
    newProcess->p_supportStruct = (support_t*)statep->gpr[26];
    newProcess->p_pid = caller->p_pid++; // uh?
}

void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST(GET_EXCEPTION_STATE_PTR(prid));
}