CC = gcc
CFLAGS = -O2 -g -std=gnu99 -I.
LDFLAGS = -lssl -lcrypto -pthread
DEPS = server.h
OBJ = server.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f server $(OBJ)