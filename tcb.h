
#ifndef TCB_H
#define TCB_H

#include "pcb.h"

typedef struct {
    PCB* pcb;
    int thread_index;
    int last_processor_id; // For load balancing in multiprocessor mode
    int preferred_processor_id; // Preferred processor for this execution cycle
} TCB;

#endif // TCB_H 