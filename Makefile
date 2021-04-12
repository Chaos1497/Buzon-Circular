CC = gcc
MAKE_STATIC_LIB = ar rv
LIB = cd ./lib &&
RM_O = cd ./lib && rm *.o

.PHONY: call

call: productor inicializador consumidor finalizador
	$(RM_O)

productor: libbchandler.a
	$(CC) -o ./bin/productor ./src/productor.c -I./include ./lib/libbchandler.a -lm -lrt -pthread

inicializador: libbchandler.a
	$(CC) -o ./bin/inicializador ./src/inicializador.c -I./include ./lib/libbchandler.a -lm -lrt -pthread

finalizador: libbchandler.a
	$(CC) -o ./bin/finalizador ./src/finalizador.c -I./include ./lib/libbchandler.a -lm -lrt -pthread
	
consumidor: libbchandler.a
	$(CC) -o ./bin/consumidor ./src/consumidor.c -I./include ./lib/libbchandler.a -lm -lrt -pthread

libbchandler.a: bchandler.o
	$(LIB) $(MAKE_STATIC_LIB) libbchandler.a bchandler.o

bchandler.o:
	$(LIB) $(CC) -c bchandler.c -I../include

