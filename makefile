CC=g++
CFLAGS=--std=c++11 -Iinclude
LDFLAGS=-levent

all: dfs dfc

tmp/server.o: src/Server.cpp
	$(CC) -c $(CFLAGS) -o $@ $<

tmp/dfs.o: src/DFS_Server.cpp tmp/server.o
	$(CC) -c $(CFLAGS) -o $@ $<

tmp/dfc.o: src/DFS_Client.cpp tmp/server.o
	$(CC) -c $(CFLAGS) -o $@ $<

dfs: tmp/dfs.o
	$(CC) -o bin/dfs $^ $(LDFLAGS)

dfc: tmp/dfc.o tmp/server.o
	$(CC) -o bin/dfc $^ $(LDFLAGS)

clean:
	rm -rf bin/dfs bin/dfc tmp/*