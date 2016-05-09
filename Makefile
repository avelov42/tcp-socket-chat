all: client server

client: client.o common.o 
	g++ client.o common.o -o client

server: server.o common.o 
	g++ server.o common.o -o server

.cpp.o:
	g++ -Wall -c $< -o $@

clean:
	rm *o
	rm client
	rm server