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

run-servers:
	./bin/dfs DFS1/ 10001 -v &
	./bin/dfs DFS2/ 10002 -v &
	./bin/dfs DFS3/ 10003 -v &
	./bin/dfs DFS4/ 10004 -v &

kill-servers:
	pgrep dfs | xargs kill

clean:
	rm -rf bin/dfs bin/dfc tmp/*