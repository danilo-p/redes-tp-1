all:
	gcc -Wall -c common.c
	gcc -Wall cliente.c common.o -o cliente
	gcc -Wall servidor.c common.o -lpthread -o servidor

clean:
	rm common.o cliente servidor

run_servidor:
	./servidor 51511

run_cliente:
	./cliente 127.0.0.1 51511
