#include "config.h"

// Variáveis globais de configuração
OperationMode operation_mode;
int num_cpus;

void init_config(void) {
    // Detecta o modo automaticamente baseado nas flags de compilação
#ifdef MULTI_MODE
    operation_mode = MODE_MULTI;
    num_cpus = 2;
#elif defined(MONO_MODE)
    operation_mode = MODE_MONO;
    num_cpus = 1;
#else
    // Fallback padrão se nenhuma flag for definida
    operation_mode = MODE_MONO;
    num_cpus = 1;
#endif
}

int is_multiprocessor_mode(void) {
    return operation_mode == MODE_MULTI;
}

int get_cpu_count(void) {
    return num_cpus;
}
