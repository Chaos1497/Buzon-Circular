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
	// Verificación de argumentos
	if(argc != 4){
		printf("\033[1;31m");
		printf("%s\n", "Error: debe insertar 3 argumentos: nombre del buffer, media de tiempos aleatorios y modo de operación");
		printf("\033[0m");
		exit(1);
	}

	iniciarConsumidor(argv[1], atoi(argv[2]), atoi(argv[3]));
	while(not(kill)) {
		// Verifica el modo de operación
		if(key_mode){
			// Esperando la tecla ENTER
			printf("\033[0;36mPresione \033[1;33mEnter\033[0;36m para consumir mensajes...\033[0m\n");
			waited_time_begin = clock();
			while(getchar() != ENTER_ASCII_CODE);
			waited_time_end = clock();
			// Calculando tiempo esperado
			consumidor.tiempoEsperado += (double)(waited_time_end - waited_time_begin) / CLOCKS_PER_SEC;
		}else{
			waited_time_begin = clock();
			// Deteniendo el proceso con valores de distribución de poisson
			sleep(poisson(consumidor.mediaTiempo));
			waited_time_end = clock();
			// Calculando tiempo esperado
			consumidor.tiempoEsperado += (double)(waited_time_end - waited_time_begin) / CLOCKS_PER_SEC;
		}
		blocked_time_begin = clock();
		// Disminuye semaforo global de consumidores
		sem_wait(consumidor.bccSemaf);
		// Almacenamiento del valor del índice del búfer de escritura del consumidor para mantenerlo intocable para otros procesos
		consumidor.indiceActualBuffer = consumidor.bcc->indiceBuffer;
		// Incrementa el indice del buffer para consumir mensajes
		consumidor.bcc->indiceBuffer++;
		consumidor.bcc->indiceBuffer = consumidor.bcc->indiceBuffer % (tamanioBloqueBuzon / sizeof(struct Mensaje));
		// Incrementa semaforo global de consumidores
		sem_post(consumidor.bccSemaf);
		// Disminuye el semáforo del búfer de mensajes del consumidor para bloquear un índice de ese búfer
		sem_wait(consumidor.bufferSemafConsumidor);
		blocked_time_end = clock();
		// Calculando tiempo bloqueado
		consumidor.tiempoBloqueado += (double)(blocked_time_end - blocked_time_begin) / CLOCKS_PER_SEC;
		// Lee un mensaje nuevo en el buffer
		leerMsg(consumidor.indiceActualBuffer);
		
	}

	// Imprime las estadísticas finales de un consumidor finalizado
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	printf("\033[1;35m|   El consumidor con id %-5i fue finalizado   \033[1;35m|\n", consumidor.PID);
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	printf("\033[0;34m|Mensajes consumidos %-10d                     \033[1;34m|\n", consumidor.mensajesConsumidos);
	printf("\033[0;34m|Tiempo esperado(s)   %-10f                     \033[1;34m|\n", consumidor.tiempoEsperado);
	printf("\033[0;34m|Tiempo bloqueado (s)  %-10f                     \033[1;34m|\n", consumidor.tiempoBloqueado);
	printf("\033[0;34m|Tiempo de usuario (us)    %-10li                 \033[1;34m|\n", consumidor.user_stime);
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	// Imprime la razón de suspensión
	printf("\033[1;31m|Razón de suspensión: %s\033[1;31m|\n", consumidor.motivoSuspension);
	return 0;
}

void iniciarConsumidor(char *nombreBuffer, int random_times_mean, int operation_mode){
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

	consumidor.PID = getpid();
	consumidor.mediaTiempo = random_times_mean;
	consumidor.modoOper = operation_mode;
	// Mapea los mensajes en la memoria compartida
	consumidor.buffer = (struct Mensaje *) mapearBloqueDeMemoria(nombreBuffer);
	// Abre el semáforo de acceso al búfer del productor y almacenar su descriptor de archivo
	char *productores_semaf_nombre = generarEtiqueta(nombreBuffer, PRODUCTOR_SEMAF_ETIQ);
	consumidor.bufferSemafProductor = abrirSemaforo(productores_semaf_nombre);
	// Abre el semáforo de acceso al búfer del consumidor y almacenar su descriptor de archivo
	char *consumidores_semaf_nombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_SEMAF_ETIQ);
	consumidor.bufferSemafConsumidor = abrirSemaforo(consumidores_semaf_nombre);
	// Mapeo del búfer de variables globales del consumidor compartido y almacenamiento de su dirección de memoria
	char *bccNombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_ETIQ);
	consumidor.bcc = (struct buzCir_consumidores *) mapearBloqueDeMemoria(bccNombre);
	// Mapeo del búfer de variables globales del productor compartido y almacenamiento de su dirección de memoria
	char *bcpNombre = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_ETIQ);
	consumidor.bcp = (struct buzCir_productores *) mapearBloqueDeMemoria(bcpNombre);
	// Mapeo del búfer de variables globales del semáforo compartido y almacenamiento de su dirección de memoria
	char *bccNombreSemaforo = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_SEMAF_ETIQ);
	consumidor.bccSemaf = abrirSemaforo(bccNombreSemaforo);
	// Disminución del semáforo del búfer global del consumidor
	sem_wait(consumidor.bccSemaf);
	// Incrementa el contador de consumidores
	consumidor.bcc->totalConsumidores++;
	consumidor.bcc->consumidoresAcumulados++;
	// Incrementa el contador del semáforo
	sem_post(consumidor.bccSemaf);
	// Almacenamiento del tamaño del búfer de mensajes compartidos para escribir el cálculo de índices
	tamanioBloqueBuzon = obtenerTamanoBloque(nombreBuffer);
	// Estadísticas y tiempos inicializados
	consumidor.mensajesConsumidos = 0;
	consumidor.tiempoEsperado = 0;
	consumidor.tiempoBloqueado = 0;
	// Estadísticas de los procesos
	getrusage(RUSAGE_SELF, &utime);
	// Liberación de nombers de semáforos, productores y consumidores
	free(consumidores_semaf_nombre);
	free(productores_semaf_nombre);
	free(bccNombre);
	free(bcpNombre);
	free(bccNombreSemaforo);
}

void leerMsg(int index){
	struct Mensaje *msg = consumidor.buffer + index;
	// Indicador de suspensión del proceso
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
	// Imprime estadísticas
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	printf("\033[1;35m| Un mensaje fue leído en la memoria compartida \033[1;35m|\n");
	printf("\033[1;33m|----------------------------------------------\033[1;33m|\n");
	printf("\033[0;34m|Indice del buffer     %-10i                 \033[1;34m|\n", consumidor.indiceActualBuffer);
	printf("\033[0;34m|Consumidores activos  %-10i                 \033[1;34m|\n", consumidor.bcc->totalConsumidores);
	printf("\033[0;34m|Productores activos  %-10i                 \033[1;34m|\n", consumidor.bcp->totalProductores);
	printf("|--------------------------------------------|\n");
	imprimirMsg(msg);
	printf("|--------------------------------------------|\033[0m\n");
}

// Imprime el mensaje consumido
void imprimirMsg(struct Mensaje *msg){
	printf("\033[0;35m|PID Productor    %-10i                 \033[1;35m|\n", msg->id);
	printf("\033[0;33m|Día              %-10i                 \033[1;33m|\n", msg->fecha.dia);
	printf("\033[0;33m|Mes              %-10i                 \033[1;33m|\n", msg->fecha.mes);
	printf("\033[0;33m|Año              %-10i                 \033[1;33m|\n", msg->fecha.anio);
	printf("\033[0;33m|Hora             %-10i                 \033[1;33m|\n", msg->tiempo.hora);
	printf("\033[0;33m|Minutos          %-10i                 \033[1;33m|\n", msg->tiempo.minutos);
	printf("\033[0;33m|Segundos         %-10i                 \033[1;33m|\n", msg->tiempo.segundos);
	printf("\033[0;33m|Número Mágico    %-10i                 \033[1;33m|\n", msg->numeroMagico);
}

// Finaliza un consumidor
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

// Distribución de Poisson
double poisson(double lambda){
	r = rand() / (RAND_MAX + 1.0);
	return log(log(r / lambda));
}
