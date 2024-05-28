CC=gcc
CFLAGS=-O2 -g -std=c99 -I.
DEPS = client.h
OBJ = client.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

client: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f client $(OBJ)