#include <bchandler.h>

void initializesFinalizer(char *nombreBuffer);
void finalizarProductor();
void escribirMsgNuevo(int index);

struct Finalizer {
	pid_t PID;
	int    mensajesProducidos;
	int    indiceActualBuffer;
	double tiempoEsperado;
	double sem_blocked_time;
	long int kernel_time;
	struct Mensaje *buffer;
	struct buzCir_productores *bcp;
	struct buzCir_consumidores *bcc;
	sem_t  *bufferSemafProductor;
	sem_t  *bufferSemafConsumidor;
	sem_t  *bcpSemaf;
	sem_t  *bccSemaf; 
} finalizador;

// Declaring semaphore names
char *productores_semaf_nombre;
char *consumidores_semaf_nombre;
char *bcpNombreSemaforo;
char *bccNombreSemaforo;

char *bcpNombre;
char *bccNombre;
struct rusage ktime;
int kill = FALSE;
int tamanioBloqueBuzon;
double r;
int consumidoresTotales;

int main(int argc, char *argv[]) {
	// Ckecking if executable file gets just 2 arguments
	if(argc != 2) {
		printf("%s\n", "\033[1;31mError: debe insertar el nombre del buffer\033[0m");
		exit(1);
	}
	initializesFinalizer(argv[1]);
	sem_wait(finalizador.bcpSemaf);
	finalizador.bcp->bufferActivo = 0;
	sem_post(finalizador.bcpSemaf);
	for (int i = 0; i <= finalizador.bcp->totalProductores; ++i){
		sem_post(finalizador.bufferSemafProductor);
	}
	while(finalizador.bcc->totalConsumidores > 0){
		// Decrement global producer bufer semaphore
		sem_wait(finalizador.bcpSemaf);
		// Storing producer writting buffer index valor for keeping untouchable for other process
		finalizador.indiceActualBuffer = finalizador.bcp->indiceBuffer;
		// This lines increments index to be written in messages buffer.
		finalizador.bcp->indiceBuffer++;
		finalizador.bcp->indiceBuffer = finalizador.bcp->indiceBuffer % (tamanioBloqueBuzon / sizeof(struct Mensaje));
		// Incrementing global producer bufer semaphorefinalizer
		sem_post(finalizador.bcpSemaf);
		// Decrementing producer messages buffer semaphore for blocking one index from that buffer
		sem_wait(finalizador.bufferSemafProductor);
		// Writing a new message into the shared buffer
		escribirMsgNuevo(finalizador.indiceActualBuffer);
		// Incrementing
		sem_post(finalizador.bufferSemafConsumidor);
	}

	// Printing in terminal a written message alarm and some statistics
	printf("\033[1;32m-------------------------------------\n");
	printf("|Estadisticas finales               |\n");
	printf("|-----------------------------------|\n");
	printf("|\033[0;32mMensajes totales       %-10i  \033[1;32m|\n", finalizador.bcp->mensajesProducidos);
	printf("|\033[0;32mConsumidores totales   %-10i  \033[1;32m|\n", finalizador.bcc->consumidoresAcumulados);
	printf("|\033[0;32mProductores totales    %-10i  \033[1;32m|\n", finalizador.bcp->productoresAcumulados);
	printf("|\033[0;33mConsumidores eliminados por llave  %-10d  \033[1;33m|\n", finalizador.bcc->llaveEliminada);
	printf("|\033[0;33mTotal de tiempo esperando      %-10f  \033[1;33m|\n", finalizador.bcc->totalTiempoEsperado + finalizador.bcp->totalTiempoEsperado);
	printf("|\033[0;33mTotal de tiempo bloqueado     %-10f  \033[1;33m|\n", finalizador.bcc->totalTiempoBloqueado + finalizador.bcp->totalTiempoBloqueado);
	printf("|\033[0;33mTiempo de usuario total (us)   %-10i  \033[1;33m|\n", finalizador.bcc->totalTiempoUsuario);
	printf("|\033[0;33mTiempo de kernel total (us) %-10i  \033[1;33m|\n", finalizador.bcp->totalTiempoKernel);
	printf("-------------------------------------\033[0m\n");
	// Setting semaphores free by nombre 
	sem_unlink(productores_semaf_nombre);
	sem_unlink(consumidores_semaf_nombre);
	sem_unlink(bccNombreSemaforo);
	sem_unlink(bcpNombreSemaforo);
	// Setting shared memory blocks free by nombre
	eliminarBloqueMemoria(bcpNombre);
	eliminarBloqueMemoria(bccNombre);
	eliminarBloqueMemoria(argv[1]);
	// Setting free used string allocated memory 
	free(productores_semaf_nombre);
	free(consumidores_semaf_nombre);
	free(bccNombreSemaforo);
	free(bcpNombreSemaforo);
	free(bcpNombre);
	free(bccNombre);
	return 0;
}

void initializesFinalizer(char *nombreBuffer) {
	// Mapping messages shared buffer address
	finalizador.buffer = (struct Mensaje *) mapearBloqueDeMemoria(nombreBuffer);
	// Opening producer buffer access semaphore and storing its file descriptor
	productores_semaf_nombre = generarEtiqueta(nombreBuffer, PRODUCTOR_SEMAF_ETIQ);
	finalizador.bufferSemafProductor = abrirSemaforo(productores_semaf_nombre);
	// Opening consumer buffer access semaphore and storing its file descriptor
	consumidores_semaf_nombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_SEMAF_ETIQ);
	finalizador.bufferSemafConsumidor = abrirSemaforo(consumidores_semaf_nombre);
	// Opening shared consumer global variables buffer semaphore and storing its file descriptor
	bccNombreSemaforo = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_SEMAF_ETIQ);
	finalizador.bccSemaf = abrirSemaforo(bccNombreSemaforo);
	// Opening shared producer global variables buffer semaphore and storing its file descriptor
	bcpNombreSemaforo = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_SEMAF_ETIQ);
	finalizador.bcpSemaf = abrirSemaforo(bcpNombreSemaforo);
	// Mapping shared producer global variables buffer and storing its memory address
	bcpNombre = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_ETIQ);
	finalizador.bcp = (struct buzCir_productores *) mapearBloqueDeMemoria(bcpNombre);
	// Mapping shared consumer global variables buffer and storing its memory address
	bccNombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_ETIQ);
	finalizador.bcc = (struct buzCir_consumidores *) mapearBloqueDeMemoria(bccNombre);
	// Decrementing global producer bufer semaphore
	// Storing shared messages buffer size for writing index computing
	tamanioBloqueBuzon = obtenerTamanoBloque(nombreBuffer);
	// Setting some timing and counting statatistic values to 0
	finalizador.mensajesProducidos = 0;
	finalizador.tiempoEsperado = 0;
	finalizador.sem_blocked_time = 0;
	finalizador.kernel_time = 0;
}

void escribirMsgNuevo(int index) {
	time_t raw_time;
 	time(&raw_time);
  	struct tm *time_info = localtime(&raw_time);
  	srand(time(NULL));
  	
	struct Mensaje msgNuevo;	
	msgNuevo.id = finalizador.PID;
	msgNuevo.fecha.dia = time_info->tm_mday;
	msgNuevo.fecha.mes = time_info->tm_mon + 1;
	msgNuevo.fecha.anio = time_info->tm_year + 1900;
	msgNuevo.tiempo.hora = time_info->tm_hour;
	msgNuevo.tiempo.minutos = time_info->tm_min;
	msgNuevo.tiempo.segundos = time_info->tm_sec;
	msgNuevo.numeroMagico = -1;
	escribirEnBloque(finalizador.buffer, &msgNuevo, sizeof(struct Mensaje), index);
}

