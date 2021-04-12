#include <bchandler.h>

void crearSemaforo(char *nombre, int valor);

int main(int argc, char *argv[]){
	if(argc != 3){
		printf("\033[1;31m");
		printf("%s\n", "Error: Debe insertar 2 argumentos, nombre y tamaño del buffer");
		printf("\033[0m");
		exit(EXIT_FAILURE);
	}
	// Inicializa el buffer y los semaforos de productores y consumidores
	char *nombreBuffer = argv[1];
	int largoBuffer = atoi(argv[2]);
	crearBloqueMemoriaCompartida(nombreBuffer, largoBuffer * sizeof(struct Mensaje));
	char *productores_semaf_nombre = generarEtiqueta(nombreBuffer, PRODUCTOR_SEMAF_ETIQ);
	crearSemaforo(productores_semaf_nombre, largoBuffer);
	char *consumidores_semaf_nombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_SEMAF_ETIQ);
	crearSemaforo(consumidores_semaf_nombre, 0);

	// Inicializa las variables compartidas del productor y sus semáforos
	struct buzCir_productores bcp;
	bcp.totalProductores = 0;
	bcp.indiceBuffer = 0;
	bcp.bufferActivo = 1;
	bcp.mensajesProducidos = 0;
    	bcp.productoresAcumulados = 0;
	bcp.totalTiempoEsperado = 0;
	bcp.totalTiempoBloqueado = 0;
	bcp.totalTiempoKernel = 0;

	char *bcpNombre = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_ETIQ);
	crearBloqueMemoriaCompartida(bcpNombre, sizeof(struct buzCir_productores));
	struct buzCir_productores * shm_producers_ptr = (struct buzCir_productores *) mapearBloqueDeMemoria(bcpNombre);
	escribirEnBloque(shm_producers_ptr, &bcp, sizeof(struct buzCir_productores), 0);
	char *bcpNombreSemaforo = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_SEMAF_ETIQ);
	crearSemaforo(bcpNombreSemaforo, 1);

	// Inicializa las variables compartidas del consumidor y sus semáforos
	struct buzCir_consumidores bcc;
	bcc.totalConsumidores = 0;
	bcc.indiceBuffer = 0;
	bcc.consumidoresAcumulados = 0;
	bcc.llaveEliminada = 0;
	bcc.totalTiempoEsperado = 0;
	bcc.totalTiempoBloqueado = 0;
	bcc.totalTiempoUsuario = 0;

	char *bccNombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_ETIQ);
	crearBloqueMemoriaCompartida(bccNombre, sizeof(struct buzCir_consumidores));
	struct buzCir_consumidores * shm_consumers_ptr = mapearBloqueDeMemoria(bccNombre);
	escribirEnBloque(shm_consumers_ptr, &bcc, sizeof(struct buzCir_consumidores), 0);
	char *bccNombreSemaforo = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_SEMAF_ETIQ);
	crearSemaforo(bccNombreSemaforo, 1);

	// Libera los nombres de semaforos, consumidores y productores
	free(productores_semaf_nombre);
	free(consumidores_semaf_nombre);
	free(bcpNombre);
	free(bcpNombreSemaforo);
	free(bccNombre);
	free(bccNombreSemaforo);
	return 0;
}

void crearSemaforo(char *nombre, int valor){
	if (sem_open(nombre, O_CREAT | O_EXCL, S_IRWXU, valor) == SEM_FAILED){
		perror("sem_open(3) error");
        exit(EXIT_FAILURE);
	}
}
