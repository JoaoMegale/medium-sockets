all: bin/common.o bin/client bin/server

bin/common.o: common.c
	gcc -Wall -c common.c -o bin/common.o

bin/client: client.c bin/common.o
	gcc -Wall client.c bin/common.o -o bin/client

bin/server: server.c bin/common.o
	gcc -Wall server.c bin/common.o -lpthread -o bin/server

clean:
	rm -f bin/common.o bin/client bin/server