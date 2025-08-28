CC = gcc
CFLAGS = -std=c99 -pthread -g

SOURCES = main.c queue.c scheduler.c log.c pcb.c config.c rbtree.c
TARGET = trabSO

all: monoprocessador

# Target para monoprocessador
monoprocessador:
	$(CC) $(CFLAGS) -DMONO_MODE -o $(TARGET) $(SOURCES)

# Target para multiprocessador  
multiprocessador:
	$(CC) $(CFLAGS) -DMULTI_MODE -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

# Testes para monoprocessador
test1_mono: monoprocessador
	./$(TARGET) casos_teste_v3/entradas/1.txt

test2_mono: monoprocessador
	./$(TARGET) casos_teste_v3/entradas/2.txt

test3_mono: monoprocessador
	./$(TARGET) casos_teste_v3/entradas/3.txt

# Testes para multiprocessador
test1_multi: multiprocessador
	./$(TARGET) casos_teste_v3/entradas/1.txt

test2_multi: multiprocessador
	./$(TARGET) casos_teste_v3/entradas/2.txt

test3_multi: multiprocessador
	./$(TARGET) casos_teste_v3/entradas/3.txt

.PHONY: all clean monoprocessador multiprocessador test1_mono test2_mono test3_mono test1_multi test2_multi test3_multi