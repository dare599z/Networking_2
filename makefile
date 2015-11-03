CC=g++
CFLAGS=--std=c++11 -Iinclude -DELPP_THREAD_SAFE
LDFLAGS=-levent

all: dfs dfc

tmp/connection.o: src/Connection.cpp
	$(CC) -c $(CFLAGS) -o $@ $<

tmp/client.o: src/Client.cpp tmp/connection.o
	$(CC) -c $(CFLAGS) -o $@ $<

tmp/server.o: src/Server.cpp
	$(CC) -c $(CFLAGS) -o $@ $<

tmp/dfs.o: src/DFS_Server.cpp tmp/server.o
	$(CC) -c $(CFLAGS) -o $@ $<

tmp/dfc.o: src/DFS_Client.cpp tmp/client.o
	$(CC) -c $(CFLAGS) -o $@ $<

dfs: tmp/dfs.o tmp/server.o
	$(CC) -o bin/dfs $^ $(LDFLAGS)

dfc: tmp/dfc.o tmp/client.o tmp/connection.o
	$(CC) -o bin/dfc $^ $(LDFLAGS) `pkg-config openssl --libs`

run:
	./bin/dfs DFS1/ 10001 -v &
	./bin/dfs DFS2/ 10002 -v &
	./bin/dfs DFS3/ 10003 -v &
	./bin/dfs DFS4/ 10004 -v &

kill:
	pgrep dfs | xargs kill

clean:
	rm -rf bin/* tmp/* logs/* DFS1/* DFS2/* DFS3/* DFS4/*