CXX=g++
CFLAGS= -Wno-unused-result -pthread

HFILES = $(shell find . -maxdepth 1 -name "*.h")

all: client server

client: client.cc $(HFILES)
	$(CXX) $(CFLAGS) $^ -o $@
server: server.cc $(HFILES)
	$(CXX) $(CFLAGS) $^ -o $@

runs: server
	./server
runc: client
	./client

clean: 
	rm -rf client server
	rm -f *~
