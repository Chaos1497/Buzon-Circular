#include <bchandler.h>

#define ENTER_ASCII_CODE 10

struct Consumidor{
	pid_t PID;
	int mediaTiempo;
	int modoOper;
	int mensajesConsumidos;
	int indiceActualBuffer;
	char * motivoSuspension;
	double tiempoEsperado;
	double tiempoBloqueado;
	long int user_stime;
	struct Mensaje *buffer;
	struct buzCir_consumidores *bcc;
	struct buzCir_productores *bcp;
	sem_t  *bccSemaf;
	sem_t  *bufferSemafProductor;
	sem_t  *bufferSemafConsumidor;
} consumidor;


struct rusage utime;
int kill = FALSE;
int key_mode = FALSE;
int tamanioBloqueBuzon;
double r;
clock_t waited_time_begin, waited_time_end;
clock_t blocked_time_begin, blocked_time_end;

void iniciarConsumidor(char *nombreBuffer, int random_times_mean, int modoOper);
void imprimirMsg(struct Mensaje *msg);
void leerMsg(int index);
void finalizarConsumidor();
double poisson(double lambda);


int main(int argc, char *argv[]){
	if(argc != 4){
		printf("\033[1;31m");
		printf("%s\n", "Error: debe insertar 3 argumentos: nombre del buffer, media de tiempos aleatorios and operation mode");
		printf("\033[0m");
		exit(1);
	}

	iniciarConsumidor(argv[1], atoi(argv[2]), atoi(argv[3]));
	// This main loop ends when kill variable is TRUE
	while(not(kill)) {
		// Checking the set mode
		if(key_mode){
			// Waiting for entering key code
			printf("\033[0;36mPresione \033[1;33mEnter\033[0;36m para consumir mensajes...\033[0m\n");
			// Saving starting waited tiempo 
			waited_time_begin = clock();
			while(getchar() != ENTER_ASCII_CODE);
			// Saving ending waited tiempo 
			waited_time_end = clock();
			// Storing defference between end and start tiempo into waited tiempo
			consumidor.tiempoEsperado += (double)(waited_time_end - waited_time_begin) / CLOCKS_PER_SEC;
		}else{
			// Saving starting waited tiempo 
			waited_time_begin = clock();
			// Stopping process with random poisson behavior values
			sleep(poisson(consumidor.mediaTiempo));
			// Saving ending waited tiempo 
			waited_time_end = clock();
			// Storing defference between end and start tiempo into waited tiempo
			consumidor.tiempoEsperado += (double)(waited_time_end - waited_time_begin) / CLOCKS_PER_SEC;
		}
		// Saving starting blocked tiempo 
		blocked_time_begin = clock();
		// Decrement global consumidor bufer semaphore
		sem_wait(consumidor.bccSemaf);
		// Storing consumidor writting buffer index valor for keeping untouchable for other process
		consumidor.indiceActualBuffer = consumidor.bcc->indiceBuffer;
		// This lines increments index to be written in messages buffer.
		consumidor.bcc->indiceBuffer++;
		consumidor.bcc->indiceBuffer = consumidor.bcc->indiceBuffer % (tamanioBloqueBuzon / sizeof(struct Mensaje));
		// Incrementing global consumidor bufer semaphore
		sem_post(consumidor.bccSemaf);
		// Decrementing consumidor messages buffer semaphore for blocking one index from that buffer
		sem_wait(consumidor.bufferSemafConsumidor);
		// Saving ending waited tiempo 
		blocked_time_end = clock();
		// Storing defference between end and start tiempo into waited tiempo
		consumidor.tiempoBloqueado += (double)(blocked_time_end - blocked_time_begin) / CLOCKS_PER_SEC;
		// Reading a new message in the shared buffer
		leerMsg(consumidor.indiceActualBuffer);
	}

	// Printing in terminal a finalized producer alarm and some statistics
	printf("\033[1;33m---------------------------------------------------\n");
	printf("|El consumidor con id %-5i fue finalizado|\n", consumidor.PID);
	printf("|-------------------------------------------------|\n");
	printf("|\033[0;33mMensajes consumidos %-10d                     \033[1;33m|\n", consumidor.mensajesConsumidos);
	printf("|\033[0;33mTiempo esperado(s)   %-10f                     \033[1;33m|\n", consumidor.tiempoEsperado);
	printf("|\033[0;33mTiempo bloqueado (s)  %-10f                     \033[1;33m|\n", consumidor.tiempoBloqueado);
	printf("|\033[0;33mTiempo de usuario (us)    %-10li                     \033[1;33m|\n", consumidor.user_stime);
	printf("---------------------------------------------------\033[0m\n");
	// Printing suspention reason
	printf("\033[1;31mRazón de suspensión: %s\033[0m\n", consumidor.motivoSuspension);
	return 0;
}

void iniciarConsumidor(char *nombreBuffer, int random_times_mean, int operation_mode){
	if (operation_mode != 0 && operation_mode != 1){
		printf("\033[1;31m");
		printf("%s\n", "Error: el modo de operación debe ser 0 (automatico) o 1 (manual)");
		printf("\033[0m");
		exit(1);
	}
	// Checking and setting the operation mode
	if(operation_mode == 1){
		key_mode = TRUE;
	}

	consumidor.PID = getpid();
	consumidor.mediaTiempo = random_times_mean;
	consumidor.modoOper = operation_mode;
	// Mapping the shared messages buffer block memory
	consumidor.buffer = (struct Mensaje *) mapearBloqueDeMemoria(nombreBuffer);
	// Opening consumidor buffer access semaphore and storing its file descriptor
	char *productores_semaf_nombre = generarEtiqueta(nombreBuffer, PRODUCTOR_SEMAF_ETIQ);
	consumidor.bufferSemafProductor = abrirSemaforo(productores_semaf_nombre);
	// Opening consumidor buffer access semaphore and storing its file descriptor
	char *consumidores_semaf_nombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_SEMAF_ETIQ);
	consumidor.bufferSemafConsumidor = abrirSemaforo(consumidores_semaf_nombre);
	// Mapping shared consumidor global variables buffer and storing its memory address
	char *bccNombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_ETIQ);
	consumidor.bcc = (struct buzCir_consumidores *) mapearBloqueDeMemoria(bccNombre);
	// Mapping shared producer global variables buffer and storing its memory address
	char *bcpNombre = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_ETIQ);
	consumidor.bcp = (struct buzCir_productores *) mapearBloqueDeMemoria(bcpNombre);
	// Opening shared productor global variables buffer semaphore and storing its file descriptor
	char *bccNombreSemaforo = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_SEMAF_ETIQ);
	consumidor.bccSemaf = abrirSemaforo(bccNombreSemaforo);
	// Decrementing global consumidor bufer semaphore
	sem_wait(consumidor.bccSemaf);
	// Incrementing total consumers valor
	consumidor.bcc->totalConsumidores++;
	consumidor.bcc->consumidoresAcumulados++;
	// Incrementing global consumidor bufer semaphore
	sem_post(consumidor.bccSemaf);
	// Storing shared messages buffer tamanio for writing index computing
	tamanioBloqueBuzon = obtenerTamanoBloque(nombreBuffer);
	// Setting some timing and counting statatistic values to 0
	consumidor.mensajesConsumidos = 0;
	consumidor.tiempoEsperado = 0;
	consumidor.tiempoBloqueado = 0;
	// Getting process statistic struct
	getrusage(RUSAGE_SELF, &utime);
	// Setting free used string allocated memory 
	free(consumidores_semaf_nombre);
	free(productores_semaf_nombre);
	free(bccNombre);
	free(bcpNombre);
	free(bccNombreSemaforo);
}

void leerMsg(int index){
	struct Mensaje *msg = consumidor.buffer + index;
	// System suspend indicator
	if (msg->numeroMagico == -1){
		finalizarConsumidor();
		consumidor.motivoSuspension = "Finalizado por el finalizador (valga la redundancia...).";
	} else if (msg->numeroMagico == consumidor.PID % 6){
		finalizarConsumidor();
		sem_wait(consumidor.bccSemaf);
		consumidor.bcc->llaveEliminada++;
		sem_post(consumidor.bccSemaf);
		consumidor.motivoSuspension = "La llave es igual al ID % 6.";
	}
	consumidor.mensajesConsumidos++;
	sem_post(consumidor.bufferSemafProductor);

	printf("\033[1;32m----------------------------------------------\n");
	printf("| Un mensaje fue leído en la memoria copartida  |\n");
	printf("|--------------------------------------------|\n");
	printf("|\033[0;32mIndice del buffer     %-10i                 \033[1;32m|\n", consumidor.indiceActualBuffer);
	printf("|\033[0;32mConsumidores activos  %-10i                 \033[1;32m|\n", consumidor.bcc->totalConsumidores);
	printf("|\033[0;32mProductores activos  %-10i                 \033[1;32m|\n", consumidor.bcp->totalProductores);
	printf("|--------------------------------------------|\n");
	imprimirMsg(msg);
	printf("----------------------------------------------\033[0m\n");
	
}

void imprimirMsg(struct Mensaje *msg){
	printf("|\033[0;32mPID Productor    %-10i                 \033[1;32m|\n", msg->id);
	printf("|\033[0;32mDía              %-10i                 \033[1;32m|\n", msg->fecha.dia);
	printf("|\033[0;32mMes              %-10i                 \033[1;32m|\n", msg->fecha.mes);
	printf("|\033[0;32mAño              %-10i                 \033[1;32m|\n", msg->fecha.anio);
	printf("|\033[0;32mHora             %-10i                 \033[1;32m|\n", msg->tiempo.hora);
	printf("|\033[0;32mMinutos          %-10i                 \033[1;32m|\n", msg->tiempo.minutos);
	printf("|\033[0;32mSegundos         %-10i                 \033[1;32m|\n", msg->tiempo.segundos);
	printf("|\033[0;32mNúmero Mágico    %-10i                 \033[1;32m|\n", msg->numeroMagico);
}

void finalizarConsumidor(){
	consumidor.user_stime = (long int) utime.ru_utime.tv_usec;
	sem_wait(consumidor.bccSemaf);
	consumidor.bcc->totalConsumidores--;
	consumidor.bcc->totalTiempoEsperado += consumidor.tiempoEsperado;
	consumidor.bcc->totalTiempoBloqueado += consumidor.tiempoBloqueado;
	consumidor.bcc->totalTiempoUsuario += consumidor.user_stime;
	sem_post(consumidor.bccSemaf);
	kill = TRUE;
}

double poisson(double lambda){
	r = rand() / (RAND_MAX + 1.0);
	return log(log(r / lambda));
}
