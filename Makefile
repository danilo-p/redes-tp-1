all:
	gcc -Wall -c common.c
	gcc -Wall client.c common.o -o client
	gcc -Wall server-mt.c common.o -lpthread -o server-mt

clean:
	rm common.o client server-mt

run_server:
	./server-mt v4 5151

run_client:
	./client 0.0.0.0 5151
