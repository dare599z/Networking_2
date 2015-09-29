CC=g++
CFLAGS=--std=c++11
LDFLAGS=-levent

all: dfs dfc

dfs: DFS_Server.cpp
	$(CC) $(CFLAGS) -o dfs $^ $(LDFLAGS)

dfc: DFS_Client.cpp
	$(CC) $(CFLAGS) -o dfc $^ $(LDFLAGS)

clean:
	rm dfs dfc