all:
	g++ -g -Wall -c common.cpp
	g++ -g -Wall cliente.cpp common.o -lpthread -o cliente
	g++ -g -Wall servidor.cpp common.o -lpthread -o servidor

clean:
	rm common.o cliente servidor

run_servidor:
	./servidor 51511

run_cliente:
	./cliente 127.0.0.1 51511

valgrind_servidor:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./servidor 51511

valgrind_cliente:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./cliente 127.0.0.1 51511

test:
	cp servidor public
	tmux new-session 'cd public; ./run-test.sh $(TEST); exec bash -i;'
