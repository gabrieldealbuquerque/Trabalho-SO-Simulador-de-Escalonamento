#ifndef CONFIG_H
#define CONFIG_H

// Modos de operação
typedef enum {
    MODE_MONO = 1,    // Monoprocessador
    MODE_MULTI = 2    // Multiprocessador
} OperationMode;

// Configuração global
extern OperationMode operation_mode;
extern int num_cpus;

// Funções de configuração
void init_config(void);
int is_multiprocessor_mode(void);
int get_cpu_count(void);

#endif // CONFIG_H
