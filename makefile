CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = escalonador
SOURCE = escalonador.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

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
