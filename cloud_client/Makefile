CC=gcc
CFLAGS=-O2 -g -std=c99 -I.
LDFLAGS = -lssl -lcrypto -pthread
DEPS = client.h
OBJ = client.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

client: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f client $(OBJ)
