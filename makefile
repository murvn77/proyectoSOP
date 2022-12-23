all: cliente gestor

gestor: gestor.o gestor.h
	gcc -o gestor gestor.o

gestor.o: gestor.c gestor.h
	gcc -c gestor.c

cliente: cliente.o cliente.h
	gcc -o cliente cliente.o -pthread

cliente.o: cliente.c cliente.h
	gcc -c cliente.c
