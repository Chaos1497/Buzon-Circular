#include <bchandler.h>

void iniciarProductor(char *nombreBuffer, double random_times_mean);
void finalizarProductor();
void escribirMsgNuevo(int index);
double exponential(double x);

struct Productor{
	pid_t  PID;
	int    mensajesProducidos;
	int    indiceActualBuffer;
	double mediaTiempo;
	double tiempoEsperado;
	double tiempoBloqueado;
	long int kernel_time;
	struct Mensaje *buffer;
	struct buzCir_productores *bcp;
	struct buzCir_consumidores *bcc;
	sem_t  *bcpSemaf;
	sem_t  *bufferSemafProductor;
	sem_t  *bufferSemafConsumidor;
} productor;

struct rusage ktime;
int kill = FALSE;
int tamanioBloqueBuzon;
double r;

clock_t waited_time_begin, waited_time_end;
clock_t blocked_time_begin, blocked_time_end;


int main(int argc, char *argv[]){
	if(argc != 3) {
		printf("%s\n", "\033[1;31mError: debe insertar 2 argumentos, nombre del buffer y media de tiempos aleatorios\033[0m");
		exit(1);
	}
	
	iniciarProductor(argv[1], atoi(argv[2]));
	while(not(kill)){
		// Saving starting waited tiempo 
		waited_time_begin = clock();
		// Stopping process with random exponential behavior values
		sleep(exponential(productor.mediaTiempo));
		// Saving ending waited tiempo 
		waited_time_end = clock();
		// Storing defference between end and start tiempo into waited tiempo
		productor.tiempoEsperado += (double)(waited_time_end - waited_time_begin) / CLOCKS_PER_SEC;
		// Saving starting blocked tiempo 
		blocked_time_begin = clock();
		// Decrement global productor bufer semaphore
		sem_wait(productor.bcpSemaf);
		// Storing productor writting buffer index valor for keeping untouchable for other process
		productor.indiceActualBuffer = productor.bcp->indiceBuffer;
		// This lines increments index to be written in messages buffer.
		productor.bcp->indiceBuffer++;
		productor.bcp->indiceBuffer = productor.bcp->indiceBuffer % (tamanioBloqueBuzon / sizeof(struct Mensaje));
		// Incrementing global productor bufer semaphore
		sem_post(productor.bcpSemaf);
		// Decrementing productor messages buffer semaphore for blocking one index from that buffer
		sem_wait(productor.bufferSemafProductor);
		// Saving ending waited tiempo 
		blocked_time_end = clock();
		// Storing defference between end and start tiempo into waited tiempo
		productor.tiempoBloqueado += (double)(blocked_time_end - blocked_time_begin) / CLOCKS_PER_SEC;
		// Writing a new message into the shared buffer
		escribirMsgNuevo(productor.indiceActualBuffer);
		// Incrementing produced messages number just for statistics
		productor.mensajesProducidos++;
		// Incrementing
		sem_post(productor.bufferSemafConsumidor);
		// Printing in terminal a written message alarm and some statistics
		printf("\033[1;32m----------------------------------------------\n");
		printf("|Un mensaje fue escrito en la memoria copartida|\n");
		printf("|----------------------------------------------|\n");
		printf("|\033[0;32mIndice del buffer    %-10i                 \033[1;32m|\n", productor.indiceActualBuffer);
		printf("|\033[0;32mConsumidores activos  %-10i                 \033[1;32m|\n", productor.bcc->totalConsumidores);
		printf("|\033[0;32mProductores activos  %-10i                 \033[1;32m|\n", productor.bcp->totalProductores);
		printf("----------------------------------------------\033[0m\n");

		if(not(productor.bcp->bufferActivo)) {
			// Calling finalize productor function
			finalizarProductor();
		}
	}

	// Printing in terminal a finalized productor alarm and some statistics
	printf("\033[1;33m---------------------------------------------------\n");
	printf("|El productor con id %-5i fue finalizado|\n", productor.PID);
	printf("|-------------------------------------------------|\n");
	printf("|\033[0;33mMensajes producidos %-10d                     \033[1;33m|\n", productor.mensajesProducidos);
	printf("|\033[0;33mTiempo esperado (s)   %-10f                     \033[1;33m|\n", productor.tiempoEsperado);
	printf("|\033[0;33mTiempo bloqueado (s)  %-10f                     \033[1;33m|\n", productor.tiempoBloqueado);
	printf("|\033[0;33mTiempo en kernel (us)  %-10li                     \033[1;33m|\n", productor.kernel_time);
	printf("---------------------------------------------------\033[0m\n");
	return 0;
}

void iniciarProductor(char *nombreBuffer, double random_times_mean) {
	// Setting productor id with the process id given by the kernel
	productor.PID = getpid();
	// Getting generating tiempo mean given by the executable argument and storing it 
	productor.mediaTiempo = random_times_mean;
	// Mapping messages shared buffer address
	productor.buffer = (struct Mensaje *) mapearBloqueDeMemoria(nombreBuffer);
	// Opening productor buffer access semaphore and storing its file descriptor
	char *productores_semaf_nombre = generarEtiqueta(nombreBuffer, PRODUCTOR_SEMAF_ETIQ);
	productor.bufferSemafProductor = abrirSemaforo(productores_semaf_nombre);
	// Opening consumer buffer access semaphore and storing its file descriptor
	char *consumidores_semaf_nombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_SEMAF_ETIQ);
	productor.bufferSemafConsumidor = abrirSemaforo(consumidores_semaf_nombre);
	// Mapping shared productor global variables buffer and storing its memory address
	char *bcpNombre = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_ETIQ);
	productor.bcp = (struct buzCir_productores *) mapearBloqueDeMemoria(bcpNombre);
	// Mapping shared consumer global variables buffer and storing its memory address
	char *bccNombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_ETIQ);
	productor.bcc = (struct buzCir_consumidores *) mapearBloqueDeMemoria(bccNombre);
	// Opening shared productor global variables buffer semaphore and storing its file descriptor
	char *bcpNombreSemaforo = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_SEMAF_ETIQ);
	productor.bcpSemaf = abrirSemaforo(bcpNombreSemaforo);
	// Decrementing global productor bufer semaphore
	sem_wait(productor.bcpSemaf);
	// Incrementing total producers valor
	productor.bcp->totalProductores++;
	productor.bcp->productoresAcumulados++;
	// Incrementing global productor bufer semaphore
	sem_post(productor.bcpSemaf);
	// Storing shared messages buffer size for writing index computing
	tamanioBloqueBuzon = obtenerTamanoBloque(nombreBuffer);
	// Setting some timing and counting statatistic values to 0
	productor.mensajesProducidos = 0;
	productor.tiempoEsperado = 0;
	productor.tiempoBloqueado = 0;
	productor.kernel_time = 0;
	// Setting free used string allocated memory 
	free(productores_semaf_nombre);
	free(bcpNombre);
	free(bccNombre);
	free(bcpNombreSemaforo);
}

void finalizarProductor() {
	getrusage(RUSAGE_SELF, &ktime);
	productor.kernel_time = (long int) ktime.ru_stime.tv_usec;
	sem_wait(productor.bcpSemaf);
	productor.bcp->totalProductores--;
	productor.bcp->mensajesProducidos += productor.mensajesProducidos;
	productor.bcp->totalTiempoEsperado += productor.tiempoEsperado;
	productor.bcp->totalTiempoBloqueado += productor.tiempoBloqueado;
	productor.bcp->totalTiempoKernel += productor.kernel_time;
	sem_post(productor.bcpSemaf);
	kill = TRUE;
}

void escribirMsgNuevo(int index) {
	time_t raw_time;
 	time(&raw_time);
  	struct tm *time_info = localtime(&raw_time);
  	srand(time(NULL));
	struct Mensaje msgNuevo;	
	msgNuevo.id = productor.PID;
	msgNuevo.fecha.dia = time_info->tm_mday;
	msgNuevo.fecha.mes = time_info->tm_mon + 1;
	msgNuevo.fecha.anio = time_info->tm_year + 1900;
	msgNuevo.tiempo.hora = time_info->tm_hour;
	msgNuevo.tiempo.minutos = time_info->tm_min;
	msgNuevo.tiempo.segundos = time_info->tm_sec;
	msgNuevo.numeroMagico = rand() % (NUMERO_MAGICO + 1);
	escribirEnBloque(productor.buffer, &msgNuevo, sizeof(struct Mensaje), index);
}

double exponential(double x) {
	r = rand() / (RAND_MAX + 1.0);
	return -log(1 - r) / x;
}
