/*  
*   Programar en C: Memoria Compartida POSIX. (Shared Memory). Linux
*   This code is a modification obtained at the website https://github.com/WhileTrueThenDream/ExamplesCLinuxUserSpace
*   Creator: WhileTrueThenDream, Mar 4th, 2020.
*/
#include "bchandler.h"
#define ERROR -1

int fd;
char *ptr;
struct stat bcObj;

// Crea un buffer de memoria compartida usando shm_open.
void crearBloqueMemoriaCompartida(char * nombreBuffer, int tamanio){
	fd = shm_open (nombreBuffer, O_CREAT | O_RDWR  ,00700); 
	if(fd == ERROR){
	   printf("Error al crear el buffer de memoria compartida.\n");
	   exit(1);
	}
	if(ftruncate(fd, tamanio) == ERROR){
	   printf("Error el buffer de memoria compartida no puede dimensionarse.\n");
	   exit(1);
	}
}

// Abre el buffer de memoria compartida.
void * mapearBloqueDeMemoria(char * nombreBuffer){
	fd = shm_open (nombreBuffer,  O_RDWR  , 00200); 
	if(fd == ERROR){
	   printf("Error file descriptor %s\n", strerror(errno));
	   exit(1);
	}
	if(fstat(fd, &bcObj) == ERROR){
	   printf("Error obteniendo estructura estática.\n");
	   exit(1);
	}
	// Mapea el buffer de memoria compartida para escribir en el mismo.
	void * ptr = mmap(NULL, bcObj.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if(ptr == MAP_FAILED){
	  printf("Mapeo fallido en el proceso: %s\n", strerror(errno));
	  exit(1);
	}
	return ptr;
}

// Abre el buffer de memoria compartida para obtener el tamaño.
int obtenerTamanoBloque(char * nombreBuffer){
	fd = shm_open (nombreBuffer,  O_RDWR  , 00200); 
	if(fd == ERROR){
	   printf("Error file descriptor %s\n", strerror(errno));
	   exit(1);
	}
	if(fstat(fd, &bcObj) == ERROR){
	   printf("Error obteniendo estructura estática.\n");
	   exit(1);
	}
	return bcObj.st_size;
}

//Copia los datos del mensaje en la posición del buffer otorgada.
void escribirEnBloque(void * ptr, void * data, int tamanio, int offset){
	memcpy(ptr + (offset * tamanio), data, tamanio);
}

// Libera el buffer de memoria compartida con shm_unlink.
void eliminarBloqueMemoria(char * nombreBuffer){
	shm_unlink(nombreBuffer);
}

int not(int boolean){
	return 1 - boolean;
}

char * generarEtiqueta(char *nombre, const char *etiqueta){
	char *nombreEtiqueta = (char *) calloc(strlen(nombre) + strlen(etiqueta), sizeof(char));
	strcpy(nombreEtiqueta, nombre);
	strcat(nombreEtiqueta, etiqueta);
	return nombreEtiqueta;
}

sem_t * abrirSemaforo(char *nombre){
	sem_t * semaphore = sem_open(nombre, O_RDWR);
	if (semaphore == SEM_FAILED){
		perror("sem_open(3) error");
        exit(EXIT_FAILURE);
	}
	return semaphore;
}
