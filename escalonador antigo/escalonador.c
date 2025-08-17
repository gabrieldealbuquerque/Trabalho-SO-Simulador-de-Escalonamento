#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSOS 100
#define MAX_THREADS 10
#define MAX_PROCESSADORES 2

typedef struct {
    int pid;
    int prioridade;
    int tempo_chegada;
    int tempo_execucao;
    int tempo_restante;
    int tempo_inicio;
    int tempo_fim;
    int threads[MAX_THREADS];
    int num_threads;
    int estado; // 0=ready, 1=running, 2=finished
} Processo;

typedef struct {
    Processo* processos[MAX_PROCESSOS];
    int inicio;
    int fim;
    int tamanho;
} Fila;

typedef struct {
    Processo* processo_atual;
    int tempo_quantum_restante;
    int ocupado;
} Processador;

// Funções da fila
void inicializar_fila(Fila* fila) {
    fila->inicio = 0;
    fila->fim = 0;
    fila->tamanho = 0;
}

int fila_vazia(Fila* fila) {
    return fila->tamanho == 0;
}

void enfileirar(Fila* fila, Processo* processo) {
    if (fila->tamanho < MAX_PROCESSOS) {
        fila->processos[fila->fim] = processo;
        fila->fim = (fila->fim + 1) % MAX_PROCESSOS;
        fila->tamanho++;
    }
}

Processo* desenfileirar(Fila* fila) {
    if (fila_vazia(fila)) {
        return NULL;
    }
    Processo* processo = fila->processos[fila->inicio];
    fila->inicio = (fila->inicio + 1) % MAX_PROCESSOS;
    fila->tamanho--;
    return processo;
}

// Funções de escalonamento
void fcfs(Fila* fila_ready, Processador* processadores, int tempo_atual, int quantum) {
    for (int i = 0; i < MAX_PROCESSADORES; i++) {
        if (!processadores[i].ocupado && !fila_vazia(fila_ready)) {
            Processo* processo = desenfileirar(fila_ready);
            processadores[i].processo_atual = processo;
            processadores[i].ocupado = 1;
            processo->estado = 1;
            if (processo->tempo_inicio == -1) {
                processo->tempo_inicio = tempo_atual;
            }
            printf("[FCFS] Executando processo PID %d // processador %d\n", processo->pid, i);
        }
    }
}

void round_robin(Fila* fila_ready, Processador* processadores, int tempo_atual, int quantum) {
    for (int i = 0; i < MAX_PROCESSADORES; i++) {
        if (!processadores[i].ocupado && !fila_vazia(fila_ready)) {
            Processo* processo = desenfileirar(fila_ready);
            processadores[i].processo_atual = processo;
            processadores[i].tempo_quantum_restante = quantum;
            processadores[i].ocupado = 1;
            processo->estado = 1;
            if (processo->tempo_inicio == -1) {
                processo->tempo_inicio = tempo_atual;
            }
            printf("[RR] Executando processo PID %d com quantum %dms // procesador %d\n", processo->pid, quantum, i);
        }
    }
}

void priority(Fila* fila_ready, Processador* processadores, int tempo_atual, int quantum) {
    for (int i = 0; i < MAX_PROCESSADORES; i++) {
        if (!processadores[i].ocupado && !fila_vazia(fila_ready)) {
            Processo* processo = desenfileirar(fila_ready);
            processadores[i].processo_atual = processo;
            processadores[i].ocupado = 1;
            processo->estado = 1;
            if (processo->tempo_inicio == -1) {
                processo->tempo_inicio = tempo_atual;
            }
            printf("[PRIORITY] Executando processo PID %d // processador %d\n", processo->pid, i);
        }
    }
}

// Função para ordenar fila por prioridade (menor número = maior prioridade)
void ordenar_fila_prioridade(Fila* fila) {
    for (int i = 0; i < fila->tamanho - 1; i++) {
        for (int j = 0; j < fila->tamanho - i - 1; j++) {
            int idx1 = (fila->inicio + j) % MAX_PROCESSOS;
            int idx2 = (fila->inicio + j + 1) % MAX_PROCESSOS;
            if (fila->processos[idx1]->prioridade > fila->processos[idx2]->prioridade) {
                Processo* temp = fila->processos[idx1];
                fila->processos[idx1] = fila->processos[idx2];
                fila->processos[idx2] = temp;
            }
        }
    }
}

// Função principal de simulação
void simular_escalonamento(Processo* processos, int num_processos, int quantum, int tipo_escalonamento) {
    Fila fila_ready;
    inicializar_fila(&fila_ready);
    
    Processador processadores[MAX_PROCESSADORES];
    for (int i = 0; i < MAX_PROCESSADORES; i++) {
        processadores[i].processo_atual = NULL;
        processadores[i].tempo_quantum_restante = 0;
        processadores[i].ocupado = 0;
    }
    
    int tempo_atual = 0;
    int processos_finalizados = 0;
    
    // Encontrar o tempo mínimo de chegada
    int tempo_min_chegada = processos[0].tempo_chegada;
    for (int i = 1; i < num_processos; i++) {
        if (processos[i].tempo_chegada < tempo_min_chegada) {
            tempo_min_chegada = processos[i].tempo_chegada;
        }
    }
    tempo_atual = tempo_min_chegada;
    
    while (processos_finalizados < num_processos) {
        // Adicionar processos que chegaram
        for (int i = 0; i < num_processos; i++) {
            if (processos[i].tempo_chegada <= tempo_atual && processos[i].estado == 0) {
                enfileirar(&fila_ready, &processos[i]);
                if (tipo_escalonamento == 3) { // PRIORITY
                    ordenar_fila_prioridade(&fila_ready);
                }
            }
        }
        
        // Executar escalonamento
        switch (tipo_escalonamento) {
            case 1: // FCFS
                fcfs(&fila_ready, processadores, tempo_atual, quantum);
                break;
            case 2: // RR
                round_robin(&fila_ready, processadores, tempo_atual, quantum);
                break;
            case 3: // PRIORITY
                priority(&fila_ready, processadores, tempo_atual, quantum);
                break;
        }
        
        // Executar processos nos processadores
        for (int i = 0; i < MAX_PROCESSADORES; i++) {
            if (processadores[i].ocupado && processadores[i].processo_atual != NULL) {
                Processo* processo = processadores[i].processo_atual;
                processo->tempo_restante--;
                
                if (tipo_escalonamento == 2) { // RR
                    processadores[i].tempo_quantum_restante--;
                }
                
                // Verificar se processo terminou
                if (processo->tempo_restante <= 0) {
                    processo->estado = 2;
                    processo->tempo_fim = tempo_atual;
                    processadores[i].ocupado = 0;
                    processadores[i].processo_atual = NULL;
                    processos_finalizados++;
                    printf("[%s] Processo PID %d finalizado\n", 
                           (tipo_escalonamento == 1) ? "FCFS" : 
                           (tipo_escalonamento == 2) ? "RR" : "PRIORITY", 
                           processo->pid);
                }
                // Verificar se quantum expirou (apenas para RR)
                else if (tipo_escalonamento == 2 && processadores[i].tempo_quantum_restante <= 0) {
                    processo->estado = 0;
                    processadores[i].ocupado = 0;
                    processadores[i].processo_atual = NULL;
                    enfileirar(&fila_ready, processo);
                }
            }
        }
        
        tempo_atual++;
    }
    
    printf("Escalonador terminou execução de todos processos\n");
}

int main() {
    int num_processos, quantum, tipo_escalonamento;
    
    // Ler entrada
    scanf("%d", &num_processos);
    scanf("%d", &quantum);
    scanf("%d", &tipo_escalonamento);
    
    printf("DEBUG: num_processos=%d, quantum=%d, tipo=%d\n", num_processos, quantum, tipo_escalonamento);
    
    Processo* processos = malloc(num_processos * sizeof(Processo));
    
    // Ler dados dos processos
    for (int i = 0; i < num_processos; i++) {
        scanf("%d", &processos[i].pid);
        scanf("%d", &processos[i].prioridade);
        scanf("%d", &processos[i].tempo_chegada);
        scanf("%d", &processos[i].tempo_execucao);
        scanf("%d", &processos[i].num_threads);
        
        printf("DEBUG: Processo %d: PID=%d, prioridade=%d, chegada=%d, execucao=%d, threads=%d\n", 
               i, processos[i].pid, processos[i].prioridade, processos[i].tempo_chegada, 
               processos[i].tempo_execucao, processos[i].num_threads);
        
        // Inicializar outros campos
        processos[i].tempo_restante = processos[i].tempo_execucao;
        processos[i].tempo_inicio = -1;
        processos[i].tempo_fim = -1;
        processos[i].estado = 0;
        
        // Ler threads (se houver)
        for (int j = 0; j < processos[i].num_threads; j++) {
            scanf("%d", &processos[i].threads[j]);
            printf("DEBUG: Thread %d do processo %d: %d\n", j, i, processos[i].threads[j]);
        }
    }
    
    // Executar simulação
    simular_escalonamento(processos, num_processos, quantum, tipo_escalonamento);
    
    free(processos);
    return 0;
} 