# trabSO — Simulador de escalonamento

Projeto acadêmico de Sistemas Operacionais em **C99**, com threads POSIX (`pthreads`), para estudar políticas de escalonamento e sincronização. A aplicação lê processos de um arquivo, simula sua execução e registra as decisões do escalonador em um log.

## O que implementa

- **FCFS**, **Round Robin**, **prioridade preemptiva** e uma implementação didática de **CFS**.
- Dois modos definidos na compilação: **monoprocessador** e **multiprocessador com duas CPUs simuladas**.
- Processos com tempo de chegada, duração, prioridade e quantidade de threads.
- Controle de estados `READY`, `RUNNING` e `FINISHED`, com mutexes e variáveis de condição.
- Fila circular para as políticas tradicionais e árvore rubro-negra para ordenar o CFS por `vruntime`.
- Log de execução, término de processos e encerramento da simulação.

## Compilar e executar

É necessário um ambiente POSIX com **GCC**, **GNU Make** e suporte a `pthreads`, como Linux ou WSL. Não há dependências de terceiros para instalar. O [makefile](makefile) usa `-std=c99 -pthread -g`.

Na raiz do repositório:

```bash
make monoprocessador
./trabSO /caminho/para/entrada.txt
```

Para simular duas CPUs:

```bash
make multiprocessador
./trabSO /caminho/para/entrada.txt
```

`make` compila o modo monoprocessador por padrão. Os dois modos geram o mesmo executável, `trabSO`: recompilar substitui a seleção anterior. O número de CPUs está definido em [config.c](config.c); não é detectado a partir do computador hospedeiro.

Para remover o executável gerado:

```bash
make clean
```

## Formato da entrada

O arquivo contém inteiros separados por espaços ou quebras de linha:

```text
quantidade_de_processos
DURACAO PRIORIDADE THREADS CHEGADA
... uma linha para cada processo ...
POLITICA
```

| Campo | Interpretação |
| --- | --- |
| `DURACAO` | Tempo de execução simulado, em milissegundos. |
| `PRIORIDADE` | Valores menores representam prioridade maior. O CFS também converte esse campo em peso. |
| `THREADS` | Quantidade de threads POSIX criadas para o processo. |
| `CHEGADA` | Tempo de chegada em milissegundos, relativo ao início do gerador. |
| `POLITICA` | `1`: FCFS; `2`: Round Robin; `3`: prioridade preemptiva; `4`: CFS. |

O Round Robin utiliza quantum fixo de **500 ms**. No CFS, a fatia considera a quantidade de itens prontos, latência alvo de **1000 ms** e granularidade mínima de **250 ms**. Esses parâmetros estão em [main.c](main.c) e [scheduler.c](scheduler.c).

### Exemplo mínimo

Crie uma entrada com dois processos de 500 ms, uma thread por processo, chegada imediata e política FCFS:

```bash
cat > /tmp/trabso-entrada.txt <<'ENTRADA'
2
500 3 1 0
500 1 1 0
1
ENTRADA

make monoprocessador
./trabSO /tmp/trabso-entrada.txt
cat escalonador_log_minikernel.txt
```

Trecho do log produzido para esse exemplo:

```text
[FCFS] Executando processo PID 1
[FCFS] Processo PID 1 finalizado
[FCFS] Executando processo PID 2
[FCFS] Processo PID 2 finalizado
Escalonador terminou execução de todos processos
```

Para experimentar outra política, altere o último inteiro da entrada. A saída é gravada em `escalonador_log_minikernel.txt` **no diretório de execução**, sobrescrevendo o log anterior. No modo multiprocessador, as mensagens de despacho incluem o identificador do processador; a ordem pode variar devido à concorrência.

## Arquitetura e arquivos

O gerador de [main.c](main.c) cria as threads conforme os tempos de chegada. Em modo mono, a fila recebe processos (`PCB`); em modo multi, recebe controles de threads (`TCB`). Os escalonadores retiram itens da fila, sinalizam sua execução e aplicam a política selecionada. As threads atualizam o tempo restante compartilhado do processo; no CFS mono, essa contagem é feita pelo próprio escalonador.

| Arquivos | Responsabilidade |
| --- | --- |
| [main.c](main.c) | Leitura da entrada, geração de processos, criação e espera das threads, gravação final do log. |
| [scheduler.c](scheduler.c), [scheduler.h](scheduler.h) | Políticas de escalonamento e execução das threads simuladas. |
| [pcb.c](pcb.c), [pcb.h](pcb.h) | Estado e recursos de cada processo, sincronização e cálculo de `vruntime`. |
| [tcb.h](tcb.h) | Controle individual das threads associadas a um processo. |
| [queue.c](queue.c), [queue.h](queue.h) | Fila de prontos, seleção por prioridade e integração com o CFS. |
| [rbtree.c](rbtree.c), [rbtree.h](rbtree.h) | Árvore rubro-negra com extração do menor `vruntime`. |
| [config.c](config.c), [config.h](config.h) | Seleção do modo e número de CPUs simuladas. |
| [log.c](log.c), [log.h](log.h) | Acúmulo de eventos em memória e gravação em arquivo. |
| [makefile](makefile) | Compilação dos dois modos e alvos de execução históricos. |

## Escopo e limitações

- É uma simulação em espaço de usuário com finalidade didática; os tempos dependem das esperas e do escalonador do sistema hospedeiro. O CFS é uma aproximação acadêmica.
- A entrada é lida com `fscanf`, sem validação completa de conteúdo, alocação ou criação de threads. Use arquivos bem formados, durações e quantidades de threads positivas e uma das quatro políticas válidas.
- A fila circular comporta até **100 itens prontos**; no modo multi, cada thread ocupa um item. Use cargas pequenas para exploração.
- O buffer de log tem **4096 bytes**; eventos que ultrapassam sua capacidade deixam de ser acrescentados. A implementação não utiliza mutex próprio para esse buffer.
- Os alvos `test1_mono` a `test3_multi` do Makefile dependem de `casos_teste_v5/entradas/`, diretório que não acompanha este repositório. Use uma entrada própria, como o exemplo acima.
- A execução de um exemplo pequeno não comprova ausência de condições de corrida, correção de todas as políticas ou comportamento sob cargas maiores.

## Contexto e créditos

Trabalho acadêmico de Sistemas Operacionais disponibilizado no repositório de [Gabriel de Albuquerque](https://github.com/gabrieldealbuquerque). O histórico Git também registra contribuições de [tiagomacsantos](https://github.com/tiagomacsantos); preserve essa atribuição ao apresentar ou adaptar o projeto.

O repositório não inclui enunciado ou relação completa de integrantes. Não há arquivo de licença versionado que estabeleça condições de reutilização.
