
#ifndef SCHEDULER
#define SCHEDULER

#include "../../headers/const.h"
#include "../../headers/types.h"
#include "../../phase1/headers/pcb.h"
#include "timers.h"
#include "initial.h" 
#include <umps/libumps.h>

#define ACQUIRE_LOCK() while(swap(&globalLock, 1))
#define RELEASE_LOCK() (globalLock = 0)
#define MIE_MTIE_MASK 0x80
#define MIE_ALL 0x888
#define MSTATUS_MIE_MASK 0x8

void scheduler();

#endif