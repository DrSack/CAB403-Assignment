CC = gcc

all: Client Server
	
Client: Client.o 

Server: Server.o

client.o: Client.c Commands.h

server.o: Server.c Commands.h
	
clean: 
	rm -f Client *.o





