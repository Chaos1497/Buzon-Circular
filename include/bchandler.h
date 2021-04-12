#ifndef _BCHANDLER_H
#define _BCHANDLER_H  

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#define PRODUCTOR_SEMAF_ETIQ		"_productores_semaf"
#define CONSUMIDOR_SEMAF_ETIQ		"_consumidores_semaf"
#define PRODUCTOR_BC_ETIQ  		"_productores_bc"
#define CONSUMIDOR_BC_ETIQ  		"_consumidores_bc"
#define PRODUCTOR_BC_SEMAF_ETIQ	"_consumidores_bc_semaf"
#define CONSUMIDOR_BC_SEMAF_ETIQ	"_productores_bc_semaf"
#define NUMERO_MAGICO			6
#define TRUE 				1
#define FALSE 				0

struct Fecha{
	int dia;
	int mes;
	int anio;
};

struct Tiempo{
	int hora;
	int minutos;
	int segundos;
};

struct Mensaje{
	pid_t id;
	struct Fecha fecha;
	struct Tiempo tiempo;
	int numeroMagico;
};

struct buzCir_productores{
	int totalProductores;
	int indiceBuffer;
	int bufferActivo;
	int mensajesProducidos;
	int productoresAcumulados;
	double totalTiempoEsperado;
	double totalTiempoBloqueado;
	int totalTiempoKernel;
};

struct buzCir_consumidores{
	int totalConsumidores;
	int indiceBuffer;
	int consumidoresAcumulados;
	int llaveEliminada;
	double totalTiempoEsperado;
	double totalTiempoBloqueado;
	int totalTiempoUsuario;
};

void crearBloqueMemoriaCompartida(char * nombreBuffer, int tamanio);
void * mapearBloqueDeMemoria(char * nombreBuffer);
void escribirEnBloque(void * ptr, void * data, int tamanio, int offset);
void eliminarBloqueMemoria(char * nombreBuffer);
int obtenerTamanoBloque(char * nombreBuffer);
char * generarEtiqueta(char *nombre, const char *etiqueta);
sem_t * abrirSemaforo(char *nombre);
int not(int boolean);

#endif
