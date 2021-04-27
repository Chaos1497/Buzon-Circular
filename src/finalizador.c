#include <bchandler.h>

void iniciarFinalizador(char *nombreBuffer);
void finalizarProductor();
void escribirMsgNuevo(int index);

struct Finalizador {
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

// Declaración de semaforos
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
	// Verificación de argumentos
	if(argc != 2) {
		printf("%s\n", "\033[1;31mError: debe insertar el nombre del buffer\033[0m");
		exit(1);
	}
	iniciarFinalizador(argv[1]);
	sem_wait(finalizador.bcpSemaf);
	finalizador.bcp->bufferActivo = 0;
	sem_post(finalizador.bcpSemaf);
	for (int i = 0; i <= finalizador.bcp->totalProductores; ++i){
		sem_post(finalizador.bufferSemafProductor);
	}
	while(finalizador.bcc->totalConsumidores > 0){
		// Disminuye semaforo global de finalizador
		sem_wait(finalizador.bcpSemaf);
		// Almacenamiento del valor del índice del búfer de escritura del finalizador para mantenerlo intocable para otros procesos
		finalizador.indiceActualBuffer = finalizador.bcp->indiceBuffer;
		// Incrementa el indice de los mensajes escritos en el buffer
		finalizador.bcp->indiceBuffer++;
		finalizador.bcp->indiceBuffer = finalizador.bcp->indiceBuffer % (tamanioBloqueBuzon / sizeof(struct Mensaje));
		// Incrementa el semaforo del finalizador del buffer
		sem_post(finalizador.bcpSemaf);
		// Disminuye el semáforo del búfer de mensajes del finalizador para bloquear un índice de ese búfer
		sem_wait(finalizador.bufferSemafProductor);
		// Escribe un mensaje nuevo en el buffer compartido
		escribirMsgNuevo(finalizador.indiceActualBuffer);
		// Incrementa el semaforo del finalizador del buffer
		sem_post(finalizador.bufferSemafConsumidor);
	}

	// Imprime estadísticas finales
	printf("\033[1;33m|-------------------------------------\033[1;33m|\n");
	printf("\033[1;35m|Estadisticas finales                 \033[1;35m|\n");
	printf("\033[1;33m|-------------------------------------\033[1;33m|\n");
	printf("\033[0;32m|Mensajes totales       %-10i  \033[1;32m|\n", finalizador.bcp->mensajesProducidos);
	printf("\033[0;32m|Consumidores totales   %-10i  \033[1;32m|\n", finalizador.bcc->consumidoresAcumulados);
	printf("\033[0;32m|Productores totales    %-10i  \033[1;32m|\n", finalizador.bcp->productoresAcumulados);
	printf("\033[0;34m|Consumidores eliminados por llave  %-10d  \033[1;34m|\n", finalizador.bcc->llaveEliminada);
	printf("\033[0;34m|Total de tiempo esperando      %-10f  \033[1;34m|\n", finalizador.bcc->totalTiempoEsperado + finalizador.bcp->totalTiempoEsperado);
	printf("\033[0;34m|Total de tiempo bloqueado     %-10f  \033[1;34m|\n", finalizador.bcc->totalTiempoBloqueado + finalizador.bcp->totalTiempoBloqueado);
	printf("\033[0;34m|Tiempo de usuario total (us)   %-10i  \033[1;34m|\n", finalizador.bcc->totalTiempoUsuario);
	printf("\033[0;34m|Tiempo de kernel total (us) %-10i  \033[1;34m|\n", finalizador.bcp->totalTiempoKernel);
	printf("-------------------------------------\033[0m\n");
	// Libera los semáforos por nombre
	sem_unlink(productores_semaf_nombre);
	sem_unlink(consumidores_semaf_nombre);
	sem_unlink(bccNombreSemaforo);
	sem_unlink(bcpNombreSemaforo);
	// Libera los bloques de memoria compartida por nombre
	eliminarBloqueMemoria(bcpNombre);
	eliminarBloqueMemoria(bccNombre);
	eliminarBloqueMemoria(argv[1]);
	// Libera los nombres de memoria y semáforos 
	free(productores_semaf_nombre);
	free(consumidores_semaf_nombre);
	free(bccNombreSemaforo);
	free(bcpNombreSemaforo);
	free(bcpNombre);
	free(bccNombre);
	return 0;
}

void iniciarFinalizador(char *nombreBuffer) {
	// Mapea los mensajes en la memoria compartida
	finalizador.buffer = (struct Mensaje *) mapearBloqueDeMemoria(nombreBuffer);
	// Abre el semáforo de acceso al búfer del productor y almacenar su descriptor de archivo
	productores_semaf_nombre = generarEtiqueta(nombreBuffer, PRODUCTOR_SEMAF_ETIQ);
	finalizador.bufferSemafProductor = abrirSemaforo(productores_semaf_nombre);
	// Abre el semáforo de acceso al búfer del consumidor y almacenar su descriptor de archivo
	consumidores_semaf_nombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_SEMAF_ETIQ);
	finalizador.bufferSemafConsumidor = abrirSemaforo(consumidores_semaf_nombre);
	// Mapeo del búfer de variables globales del consumidor compartido y almacenamiento de su dirección de memoria
	bccNombreSemaforo = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_SEMAF_ETIQ);
	finalizador.bccSemaf = abrirSemaforo(bccNombreSemaforo);
	// Mapeo del búfer de variables globales del productor compartido y almacenamiento de su dirección de memoria
	bcpNombreSemaforo = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_SEMAF_ETIQ);
	finalizador.bcpSemaf = abrirSemaforo(bcpNombreSemaforo);
	// Mapeo del búfer de variables globales del productor compartido y almacenamiento de su dirección de memoria
	bcpNombre = generarEtiqueta(nombreBuffer, PRODUCTOR_BC_ETIQ);
	finalizador.bcp = (struct buzCir_productores *) mapearBloqueDeMemoria(bcpNombre);
	// Mapeo del búfer de variables globales del consumidor compartido y almacenamiento de su dirección de memoria
	bccNombre = generarEtiqueta(nombreBuffer, CONSUMIDOR_BC_ETIQ);
	finalizador.bcc = (struct buzCir_consumidores *) mapearBloqueDeMemoria(bccNombre);
	// Almacenamiento del tamaño del búfer de mensajes compartidos para escribir el cálculo de índices
	tamanioBloqueBuzon = obtenerTamanoBloque(nombreBuffer);
	// Estadísticas y tiempos inicializados
	finalizador.mensajesProducidos = 0;
	finalizador.tiempoEsperado = 0;
	finalizador.sem_blocked_time = 0;
	finalizador.kernel_time = 0;
}

// Escribe el mensaje de finalización a los productores y consumidores
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

