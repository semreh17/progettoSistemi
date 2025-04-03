void exceptionHandler() {

}



void uTLB_RefillHandler() {         //fuzione fornita neelle spec
int prid = getPRID();              
5
setENTRYHI(0x80000000);
setENTRYLO(0x00000000);
TLBWR();
LDST(GET_EXCEPTION_STATE_PTR(prid));
}