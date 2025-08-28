
#ifndef TCB_H
#define TCB_H

#include "pcb.h"

typedef struct {
    PCB* pcb;
    int thread_index;
    int last_processor_id; 
    int preferred_processor_id; 
} TCB;

#endif 