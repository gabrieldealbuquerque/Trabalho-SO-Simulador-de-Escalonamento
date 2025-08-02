
#ifndef TCB_H
#define TCB_H

#include "pcb.h"

typedef struct {
    PCB* pcb;
    int thread_index;
} TCB;

#endif // TCB_H 