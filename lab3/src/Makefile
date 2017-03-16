CC=gcc
CFLAGS= -Wno-unused-result

HFILES = $(shell find . -maxdepth 1 -name "*.h")

all: client server

client: client.c $(HFILES)
	$(CC) $(CFLAGS) $^ -o $@
server: server.c $(HFILES)
	$(CC) $(CFLAGS) $^ -o $@

runs: server
	./server
remote-run: client
	./client 114.212.191.33
local-run: client
	./client 127.0.0.1

clean: 
	rm -rf client server
	rm -f *~
