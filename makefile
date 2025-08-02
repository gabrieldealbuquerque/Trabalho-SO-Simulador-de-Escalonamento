CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread

SOURCES = main.c queue.c scheduler.c log.c

all: monoprocessador multiprocessador

monoprocessador: $(SOURCES)
	$(CC) $(CFLAGS) -DMONO -o trabSO_mono $(SOURCES)

multiprocessador: $(SOURCES)
	$(CC) $(CFLAGS) -DMULTI -o trabSO_multi $(SOURCES)

clean:
	rm -f trabSO_mono trabSO_multi

test1: $(TARGET)
	./$(TARGET) < casos_teste_v3/entradas/1.txt

test2: $(TARGET)
	./$(TARGET) < casos_teste_v3/entradas/2.txt

test3: $(TARGET)
	./$(TARGET) < casos_teste_v3/entradas/3.txt

test4: $(TARGET)
	./$(TARGET) < casos_teste_v3/entradas/4.txt

test5: $(TARGET)
	./$(TARGET) < casos_teste_v3/entradas/5.txt

.PHONY: all clean test1 test2 test3 test4 test5
