CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2

OBJ = main.o linha.o
BIN = linha_bolacha

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ)

main.o: main.c linha.h
	$(CC) $(CFLAGS) -c main.c

linha.o: linha.c linha.h
	$(CC) $(CFLAGS) -c linha.c

clean:
	rm -f $(OBJ) $(BIN)
