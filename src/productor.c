#include <bchandler.h>

#define ENTER_ASCII_CODE 10

void iniciarProductor(char *nombreBuffer, int random_times_mean, int modoOper);
void finalizarProductor();
void escribirMsgNuevo(int index);
double exponential(double x);

struct Productor{
	pid_t  PID;
	int    mensajesProducidos;
	int    indiceActualBuffer;
	int    modoOper;
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
int key_mode = FALSE;
int tamanioBloqueBuzon;
double r;

clock_t waited_time_begin, waited_time_end;
clock_t blocked_time_begin, blocked_time_end;


int main(int argc, char *argv[]){
	// Verificación de argumentos
	if(argc != 4){
		printf("\033[1;31m");
		printf("%s\n", "Error: debe insertar 3 argumentos: nombre del buffer, media de tiempos aleatorios y modo de operación");
		printf("\033[0m");
		exit(1);
	}
	
	iniciarProductor(argv[1], atoi(argv[2]), atoi(argv[3]));
	while(not(kill)) {
		// Verifica el modo de operación
		if(key_mode){
			// Esperando la tecla ENTER
			printf("\033[0;36mPresione \033[1;33mEnter\033[0;36m para producir mensajes...\033[0m\n"); 
			waited_time_begin = clock();
			while(getchar() != ENTER_ASCII_CODE);
			waited_time_end = clock();
			// Calculando tiempo esperado
			productor.tiempoEsperado += (double)(waited_time_end - waited_time_begin) / CLOCKS_PER_SEC;
		}else{ 
			waited_time_begin = clock();
			// Deteniendo el proceso con valores de distribución exponencial
			sleep(exponential(productor.mediaTiempo)); 
			waited_time_end = clock();
			// Calculando tiempo esperado
			productor.tiempoEsperado += (double)(waited_time_end - waited_time_begin) / CLOCKS_PER_SEC;
		}
		blocked_time_begin = clock();
		// Disminuye semaforo global de productores
		sem_wait(productor.bcpSemaf);
		// Almacenamiento del valor del índice del búfer de escritura del productor para mantenerlo intocable para otros procesos
		productor.indiceActualBuffer = productor.bcp->indiceBuffer;
		// Incrementa el indice del buffer para escribir mensajes
		productor.bcp->indiceBuffer++;
		productor.bcp->indiceBuffer = productor.bcp->indiceBuffer % (tamanioBloqueBuzon / sizeof(struct Mensaje));
		// Incrementa semaforo global de productores
		sem_post(productor.bcpSemaf);
		// Disminuye el semáforo del búfer de mensajes del consumidor para bloquear un índice de ese búfer
		sem_wait(productor.bufferSemafProductor);
		blocked_time_end = clock();
		// Calculando tiempo bloqueado
		productor.tiempoBloqueado += (double)(blocked_time_end - blocked_time_begin) / CLOCKS_PER_SEC;
		// Escribe un mensaje nuevo en el buffer
		escribirMsgNuevo(productor.indiceActualBuffer);
		// Incrementa los mensajes producidos en las estadisticas
		productor.mensajesProducidos++;
		// Incrementa el semaforo del consumidor del buffer
		sem_post(productor.bufferSemafConsumidor);
		// Imprime estadísticas
		printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
		printf("\033[1;35m|Un mensaje fue escrito en la memoria copartida\033[1;35m|\n");
		printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
		printf("\033[0;34m|Indice del buffer    %-10i                 \033[1;34m|\n", productor.indiceActualBuffer);
		printf("\033[0;34m|Consumidores activos  %-10i                 \033[1;34m|\n", productor.bcc->totalConsumidores);
		printf("\033[0;34m|Productores activos  %-10i                 \033[1;34m|\n", productor.bcp->totalProductores);
		printf("----------------------------------------------\033[0m\n");

		if(not(productor.bcp->bufferActivo)) {
			// Llama al finalizador de productores
			finalizarProductor();
		}
	}

	// Imprime las estadísticas finales de un productor finalizado
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	printf("\033[1;35m|El productor con id %-5i fue finalizado  \033[1;35m|\n", productor.PID);
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	printf("\033[0;34m|Mensajes producidos %-10d                     \033[1;34m|\n", productor.mensajesProducidos);
	printf("\033[0;34m|Tiempo esperado (s)   %-10f                    \033[1;34m|\n", productor.tiempoEsperado);
	printf("\033[0;34m|Tiempo bloqueado (s)  %-10f                     \033[1;34m|\n", productor.tiempoBloqueado);
	printf("\033[0;34m|Tiempo en kernel (us)  %-10li                    \033[1;34m|\n", productor.kernel_time);
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	return 0;
}

void iniciarProductor(char *nombreBuffer, int random_times_mean, int operation_mode){
	// Verificación de argumentos
	if (operation_mode != 0 && operation_mode != 1){
		printf("\033[1;31m");
		printf("%s\n", "Error: el modo de operación debe ser 0 (automatico) o 1 (manual)");
		printf("\033[0m");
		exit(1);
	}
	// Verificación del modo de operación
	if(operation_mode == 1){
		key_mode = TRUE;
	}

	productor.PID = getpid();
	productor.mediaTiempo = random_times_mean;
	productor.modoOper = operation_mode;
	// Mapea los mensajes en la memoria compartida
	productor.buffer = (struct Mensaje *) mapearBloqueDeMemoria(nombreBuffer);
	// Abre el semáforo de acceso al búfer del productor y almacenar su descriptor de archivo
	char *productores_semaf_nombre = generarEtiqueta(nombreBuffer, PRODUCTOR_SEMAF_ETIQ);
	productor.bufferSemafProductor = abrirSemaforo(productores_semaf_nombre);
	// Abre el semáforo de acceso al búfer del consumidor y almacenar su descriptor de archivo
	char *consumidores_semaf_nombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_SEMAF_ETIQ);
	productor.bufferSemafConsumidor = abrirSemaforo(consumidores_semaf_nombre);
	// Mapeo del búfer de variables globales del productor compartido y almacenamiento de su dirección de memoria
	char *bcpNombre = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_ETIQ);
	productor.bcp = (struct buzCir_productores *) mapearBloqueDeMemoria(bcpNombre);
	// Mapeo del búfer de variables globales del consumidor compartido y almacenamiento de su dirección de memoria
	char *bccNombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_ETIQ);
	productor.bcc = (struct buzCir_consumidores *) mapearBloqueDeMemoria(bccNombre);
	// Mapeo del búfer de variables globales del semáforo compartido y almacenamiento de su dirección de memoria
	char *bcpNombreSemaforo = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_SEMAF_ETIQ);
	productor.bcpSemaf = abrirSemaforo(bcpNombreSemaforo);
	// Disminución del semáforo del búfer global del consumidor
	sem_wait(productor.bcpSemaf);
	// Incrementa el contador de productores
	productor.bcp->totalProductores++;
	productor.bcp->productoresAcumulados++;
	// Incrementa el contador del semáforo
	sem_post(productor.bcpSemaf);
	// Almacenamiento del tamaño del búfer de mensajes compartidos para escribir el cálculo de índices
	tamanioBloqueBuzon = obtenerTamanoBloque(nombreBuffer);
	// Estadísticas y tiempos inicializados
	productor.mensajesProducidos = 0;
	productor.tiempoEsperado = 0;
	productor.tiempoBloqueado = 0;
	productor.kernel_time = 0;
	// Liberación de nombers de semáforos, productores y consumidores
	free(productores_semaf_nombre);
	free(bcpNombre);
	free(bccNombre);
	free(bcpNombreSemaforo);
}

// Finaliza un productor
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

// Escribe un mensaje nuevo en el bloque de memoria compartida
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
	printf("|--------------------------------------------|\n");
	printf("\033[0;35m|PID Productor    %-10i                 \033[1;35m|\n", msgNuevo.id);
	printf("\033[0;33m|Día              %-10i                 \033[1;33m|\n", msgNuevo.fecha.dia);
	printf("\033[0;33m|Mes              %-10i                 \033[1;33m|\n", msgNuevo.fecha.mes);
	printf("\033[0;33m|Año              %-10i                 \033[1;33m|\n", msgNuevo.fecha.anio);
	printf("\033[0;33m|Hora             %-10i                 \033[1;33m|\n", msgNuevo.tiempo.hora);
	printf("\033[0;33m|Minutos          %-10i                 \033[1;33m|\n", msgNuevo.tiempo.minutos);
	printf("\033[0;33m|Segundos         %-10i                 \033[1;33m|\n", msgNuevo.tiempo.segundos);
	printf("\033[0;33m|Número Mágico    %-10i                 \033[1;33m|\n", msgNuevo.numeroMagico);
	printf("|--------------------------------------------|\033[0m\n");
	escribirEnBloque(productor.buffer, &msgNuevo, sizeof(struct Mensaje), index);

}

//Distribución exponencial
double exponential(double x) {
	r = rand() / (RAND_MAX + 1.0);
	return -log(1 - r) / x;
}
