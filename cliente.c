/*
Archivo: cliente.c
Realizado por: Juan Jaimes y Mauren Rivera - [Camilo Rodríguez]
Contiene: implementación de funciones para:
--Pipes: creación en modo acoplado y desacoplado
--Mostrar: menu, tweets y twees en modo acoplado
--Leer: tweets del cliente y respuestas
--Crear hilo
--Solicitar una funcionalidad al gestor
Fecha última modificación: 19/11/2022
*/

#include "cliente.h"

#include "gestor.h"
#include "cliente.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

Cliente cliente;
bool modoGestor;
char *pipeGestor, pipeCliente[NOMPID] = "pipe-", pipeTweet[NOMPID] = "pipeTweet-";
pthread_t thread;
sem_t sem;

mode_t fifo_mode = S_IRUSR | S_IWUSR;

/*
Función: main
Parámetros de Entrada: entero con la cantidad de parámetros, doble apuntador con los parámetros de entrada
Valor de salida: entero para determinar cómo terminó el programa
Descripción: programa principal del archivo para empezar el llamado de funciones, crear pipes y llevar a cabo las solicitudes del cliente
*/

int main(int argc, char **argv) {
    int idClienteFollow, idClienteUnfollow, cantCar, opcion = 0;
    bool registrado, desconexion, follow, unfollow, tweet, salir = false;

    argumentos(argc, argv);

    sprintf(cliente.PID, "%d", getpid());
    strcat(pipeCliente, cliente.PID);
    strcat(pipeTweet, cliente.PID);

    crearPipe();
    solicitud(0);

    registrado = leerRespuesta();

    if (modoGestor&& registrado) {
        crearPipeAcoplado();
        crearHilo();
        //Leer los tweets que recibio mientras estaba desconectado
        solicitud(4);
    }
    //Condición de que el usuario sí se haya registrado!
    while (registrado && !salir) {
        menu();
        scanf("%d", &opcion);
        switch (opcion) {
            case 1:
                printf("Digite el id del usuario a seguir: ");
                scanf("%d", &idClienteFollow);
                cliente.idExterno = idClienteFollow;
                solicitud(1);
                follow = leerRespuesta();
                break;

            case 2:
                printf("Digite el id del usuario a dejar de seguir: ");
                scanf("%d", &idClienteUnfollow);
                cliente.idExterno = idClienteUnfollow;
                solicitud(2);
                unfollow = leerRespuesta();
                break;

            case 3:
                cantCar = 0;
                printf("Digite el mensaje del tweet: \n");
                fgets(cliente.tweetUsuario.tweet, TAMTWEET, stdin);
                cliente.tweetUsuario.idUsuario = cliente.id;
                printf("\n%s\n", cliente.tweetUsuario.tweet);
                while (cliente.tweetUsuario.tweet[cantCar] != '\0') {
                    cantCar++;
                }
                if (cantCar <= 200) {
                    solicitud(3);
                    tweet = leerRespuesta();
                } else {
                    printf("El mensaje no puede contener más de 200 carácteres");
                }
                break;

            case 4:
                if (!modoGestor) {
                    solicitud(4);
                    leerTweetCliente();
                    mostrarTweets();
                } else
                    printf("\nNo se puede acceder a esta opcion ya que el gestor esta en modo acoplado.\n");
                break;

            case 5:
                // Llamada de función de gestor.c desconexion
                solicitud(5);
                desconexion = leerRespuesta();
                salir = true;
                printf("\nHasta pronto!\n\n");
                break;

            default:
                printf("ERROR: Opción invalida:\n");
                break;
        }
    }
    unlink (pipeTweet);
    unlink(pipeCliente);
    return 0;
}

/*
Función: CrearHilo
Parámetros de Entrada: no tiene
Valor de salida: no tiene
Descripción: creación de hilo para que esté en constante revisión frente a los tweets que envíen otros clientes
*/

void crearHilo () {
    int rc;
    rc = pthread_create (&thread, NULL, (void *) mostrarTweetsAcoplado, NULL);
    if (rc) {
        perror ("No se pudo crear el hilo.");
        exit (-1);
    }
}

/*
Función: MostrarTweetsAcoplado
Parámetros de Entrada: no tiene
Valor de salida: no tiene
Descripción: muestra los tweets en el modo acoplado cuando hay un nuevo tweet
*/

void mostrarTweetsAcoplado () {
    while (true) {
        leerTweetCliente();
        if (*(cliente.tweetsRecibidos + 0)->tweet != '\0') {
            system ("clear");
            mostrarTweets();
            menu();
        }
    }
}

/*
Función: Argumentos
Parámetros de Entrada: entero con la cantidad de parámetros, doble apuntador con los parámetros de entrada
Valor de salida: no tiene
Descripción: se asignan flags y valida que los argumentos de entrada sean válidos 
*/

void argumentos (int argc, char **argv) {
    int opt;
    if (argc != 5) {
        printf("Ingrese los argumentos correctos.\n");
        printf("Formato de entrada: cliente -i ID -p pipeNom. (no importa el orden de los args).\n");
        exit(-1);
    }
    while((opt = getopt(argc, argv, ":i:p:")) != -1)
    {
        switch(opt)
        {
            case 'i':
                cliente.id = atoi(optarg);
                break;
            case 'p':
                pipeGestor = optarg;
                break;
            case ':':
                printf("El argumento %c necesita un valor.\n", opt);
                exit (-1);
            case '?':
                printf("Argumento desconocido: %c\n", optopt);
                exit (-1);
        }
    }
}

/*
Función: Menu
Parámetros de Entrada: no tiene
Valor de salida: no tiene
Descripción: muestra el menú con las acciones que puede solicitar el cliente
*/

void menu() {
    printf("\n------------------------MENÚ------------------------\n");
    printf(" Digite el número de una de las siguientes opciones:\n");
    printf("1. Follow\n");
    printf("2. Unfollow\n");
    printf("3. Tweet\n");
    printf("4. Recuperar tweets\n");
    printf("5. Desconexión\n");
}

/*
Función: MostrarTweets
Parámetros de Entrada: no tiene
Valor de salida: no tiene
Descripción: muestra los tweets que tiene un cliente
*/

void mostrarTweets () {
    bool encontro = false, tweets = false;
    printf ("\n-------- NUEVOS TWEETS --------");
    for (int id = 1; id <= MAXUSERS;) {
        for (int i = 0; i < TAMTWEET; ++i) {
            if (cliente.tweetsRecibidos[i].idUsuario == id) {
                encontro = true;
                printf("\nUsuario %d:\n", cliente.tweetsRecibidos[i].idUsuario);
                for (int k = 0; k < TAMTWEET; ++k) {
                    if (cliente.tweetsRecibidos[k].idUsuario == id &&
                        *(cliente.tweetsRecibidos + k)->tweet != '\0') {
                        printf("  - %s", cliente.tweetsRecibidos[k].tweet);
                        tweets = true;
                    }
                }
                id++;
            } else
                encontro = false;
        }
        if (!encontro) {
            id++;
        }
    }
    if (!tweets) printf ("\nNo hay tweets por mostrar.\n");
}

/*
Función: CrearPipe
Parámetros de Entrada: no tiene
Valor de salida: booleano para saber si la creación del pipe fue exitosa
Descripción: crea el pipe de comunicación por parte del cliente
*/

bool crearPipe() {
    bool realizado = true;
    if (mkfifo(pipeCliente, fifo_mode) == -1) {
        perror("Client mkfifo");
        exit(-1);
    } else
        return realizado;
}

/*
Función: CrearPipeAcoplado
Parámetros de Entrada: no tiene
Valor de salida: booleano para saber si la creación del pipe fue exitosa
Descripción: crea el pipe de comunicación por parte del cliente en el modo acoplado, a través de éste recibe los tweets que le van enviando
*/

bool crearPipeAcoplado () {
    bool realizado = true;
    if (mkfifo(pipeTweet, fifo_mode) == -1) {
        perror("Client mkfifo");
        exit(-1);
    } else
        return realizado;
}

/*
Función: LeerTweetCliente
Parámetros de Entrada: no tiene
Valor de salida: no tiene
Descripción: dependiendo del modo del gestor, el cliente abrirá el pipe correspondiente para leer la estructura de los tweets y copiar el contenido de los mismos a la estructura del cliente
*/

void leerTweetCliente() {
    int fdPipeCliente, leer;
    Cliente recibido;

    if (modoGestor) {
        fdPipeCliente = open(pipeTweet, O_RDONLY);
    } else
        fdPipeCliente = open(pipeCliente, O_RDONLY);

    if (fdPipeCliente == -1) {
        perror("Apertura del pipe");
    }
    leer = read(fdPipeCliente, &recibido, sizeof(Cliente));
    if (leer == -1) {
        perror("Lectura del pipe del usuario");
        exit(-1);
    }

    memcpy(cliente.tweetsRecibidos, recibido.tweetsRecibidos, sizeof(cliente.tweetsRecibidos));
    close(fdPipeCliente);
}

/*
Función: LeerRespuesta
Parámetros de Entrada: no tiene
Valor de salida: booleano para confirmar realización (éxitosa o no) de las operaciones solicitadas por el usuario
Descripción: determina si el mensaje por parte del gestor establece que la operación solicitada por el usuario fue éxitosa o no
*/

bool leerRespuesta() {
    bool registro;
    int fdPipeCliente, leer;
    Mensaje confirmacion;
    fdPipeCliente = open(pipeCliente, O_RDONLY);
    if (fdPipeCliente == -1) {
        perror("Apertura del pipe del usuario");
        exit(-1);
    }
    leer = read(fdPipeCliente, &confirmacion, sizeof(Mensaje));
    if (leer == -1) {
        perror("Lectura del pipe del usuario");
        exit(-1);
    }
    printf("%s\n", confirmacion.mensaje);
    registro = confirmacion.confirmacion;
    modoGestor = confirmacion.modoGestor;
    close(fdPipeCliente);
    return registro;
}

/*
Función: Solicitud
Parámetros de Entrada: entero con la opción del menú a solicitar
Valor de salida: no tiene
Descripción: se hace apertura del pipe gestor y se escribe el número de la opción de la solicitud
*/


void solicitud(int opcion) {
    int fd, creado = 0, escribir;
    cliente.opcion = opcion;
    do {
        fd = open(pipeGestor, O_WRONLY);
        if (fd == -1) {
            perror("pipe del gestor");
            printf(" Se volvera a intentar en unos segundos...\n");
            sleep(10);
        } else creado = 1;
    } while (creado == 0);
    escribir = write(fd, &cliente, sizeof(cliente));
    if (escribir == -1) {
        perror("escritura en el pipe del gestor");
    }
    close(fd);
}

