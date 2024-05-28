CC=gcc
CFLAGS=-O2 -g -std=gnu99 -I.
LDFLAGS=-pthread
DEPS = server.h
OBJ = server.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f server $(OBJ)