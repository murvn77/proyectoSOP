/*
Archivo: gestor.c
Realizado por: Juan Jaimes y Mauren Rivera - [Camilo Rodríguez]
Contiene: implementación de funciones para:
--Inicializar conexiones, validación argumentos
--Usuarios: inicializar, registrar, seguir, dejar de seguir, desconectar y encontrar usuarios conectados o desconectados
--Mostrar: mensajes de confirmación, conexiones y estadísticas
--Tweets: leer, encontrar posición, enviar tweet acoplado, limpiar
--Alarma: crear
Fecha última modificación: 19/11/2022
*/

#include "gestor.h"
#include "cliente.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <semaphore.h>

Gestor gestor;
Estadisticas estadisticas;
Cliente usuarios[MAXUSERS];
struct itimerval contador;

mode_t fifo_mode = S_IRUSR | S_IWUSR;

/*
Función: main
Parámetros de Entrada: entero con la cantidad de parámetros, doble apuntador con los parámetros de entrada
Valor de salida: entero para determinar cómo terminó el programa
Descripción: programa principal del archivo para empezar el llamado de funciones, crear pipes y llevar a cabo las solicitudes del cliente
*/

int main(int argc, char **argv) {
    bool cargado, registrado, desconectado, followUser, unfollowUser, tweetLeido, tweetEnviado;
    Cliente cliente;

    argumentos(argc, argv);

    crearAlarma ();

    unlink(gestor.pipeConexion);
    if (mkfifo(gestor.pipeConexion, fifo_mode) == -1) {
        perror("Server mkfifo");
        exit(-1);
    }

    cargado = inicializarConexiones();

    mostrarConexiones();

    while (true && cargado) {
        cliente = leerPipe();
        switch (cliente.opcion) {
            case 0:
                registrado = registroUsuario(cliente);
                estadisticas.usuariosConectados++; //Aumenta el contaodor de los users conectados para las estadisticas
                break;
            case 1:
                followUser = follow(cliente);
                mostrarConexiones();
                break;
            case 2:
                unfollowUser = unfollow(cliente);
                mostrarConexiones();
                break;
            case 3:
                tweetLeido = leerTweet(cliente);
                if (gestor.modo == 'A') {
                    enviarTweetAcoplado(cliente);
                }
                estadisticas.tweetsRecibidos++; // aumenta en uno los tweets recibidos para las estadisticas
                mensajeConfirmacion(0, tweetLeido, cliente, 3);
                break;
            case 4:
                tweetEnviado = enviarTweet(cliente);
                if (gestor.modo == 'D') {
                    estadisticas.tweetsEnviados++; // aumenta en uno los tweets enviados para las estadisticas en modo desacoplado
                }
                limpiarTweets(encontrarUsuarioConectado(cliente.id));
                break;
            case 5:
                desconectado = desconexionUsuario(cliente);
                break;
            default:
                break;
        }
    }
    return 0;
}

/*
Función: Argumentos
Parámetros de Entrada: entero con la cantidad de parámetros, doble apuntador con los parámetros de entrada
Valor de salida: no tiene
Descripción: se asignan flags y valida que los argumentos de entrada sean válidos 
*/

void argumentos (int argc, char **argv) {
    int opt;
    if (argc != 11) {
        printf("Ingrese los argumentos correctos.\n");
        printf("Formato de entrada: gestor -n Num -r Relaciones -m modo -t time -p pipenom. (no importa el orden de los args).\n");
        exit(-1);
    }
    while((opt = getopt(argc, argv, ":n:r:m:t:p:")) != -1)
    {
        switch(opt)
        {
            case 'n':
                gestor.maxUsers = atoi(optarg);
                break;
            case 'r':
                gestor.relaciones = optarg;
                break;
            case 'm':
                gestor.modo = optarg[0];
                break;
            case 't':
                gestor.time = atoi(optarg);
                break;
            case 'p':
                gestor.pipeConexion = optarg;
                break;
            case ':':
                printf("El argumento %c necesita un valor.\n", opt);
                exit (-1);
            case '?':
                printf("Argumento desconocido: %c\n", optopt);
                exit (-1);
                break;
        }
    }
}

/*
Función: CrearAlarma
Parámetros de Entrada: no tiene
Valor de salida: no tiene
Descripción: se crea la alarma dependiendo del time del parámetro para mostrar estadísticas periódicamente
*/

void crearAlarma () {
    struct timeval tiempoInicial, tiempoRepeticion;
    tiempoInicial.tv_sec = gestor.time;
    tiempoInicial.tv_usec = 0;
    tiempoRepeticion.tv_sec = gestor.time;
    tiempoRepeticion.tv_usec = 0;
    contador.it_value=tiempoInicial;
    contador.it_interval=tiempoRepeticion;

    if (signal(SIGALRM, imprimirEstadisticas) == SIG_ERR) {
        perror ("No se pudo establecer la alarma.");
    }

    setitimer (ITIMER_REAL, &contador, NULL);
}

/*
Función: ImprimirEstadisticas
Parámetros de Entrada: entero con el id de la señal
Valor de salida: no tiene
Descripción: imprime las estadísticas dependiendo del comportamiento que haya en el sistema
*/

void imprimirEstadisticas (int idSignal) {
    printf ("\n----- ESTADISTICAS -----\n");
    printf ("Usuarios conectados : %d", estadisticas.usuariosConectados);
    printf ("\nTweets enviados : %d", estadisticas.tweetsEnviados);
    printf ("\nTweets recibidos : %d\n\n", estadisticas.tweetsRecibidos);
}

/*
Función: LeerPipe
Parámetros de Entrada: no tiene
Valor de salida: Cliente
Descripción: hace la apertura del pipe y la lectura
*/

Cliente leerPipe() {
    int leer, fd;
    Cliente cliente;
    fd = open(gestor.pipeConexion, O_RDONLY);
    if (fd == -1) {
        perror("Apertura del pipe");
    }
    leer = read(fd, &cliente, sizeof(cliente));
    if (leer == -1) {
        perror("lectura del pipe");
    }
    close(fd);
    return cliente;
}

/*
Función: InicializarConexiones
Parámetros de Entrada: no tiene
Valor de salida: bool 
Descripción: inicializa la matriz de conexiones dependiendo del archivo que se establezca en el parámetro
*/

bool inicializarConexiones() {
    FILE *fd = NULL;
    bool realizado;
    int valor, i = 0, j;
    char *cadena = malloc(160 * sizeof(char)), *token, del[] = " ";
    fd = fopen(gestor.relaciones, "r");
    if (fd == NULL) {
        perror("Apertura de archivo");
        exit(-1);
    }
    while (fgets(cadena, 160, fd) != NULL) {
        token = strtok(cadena, " ");
        j = 0;
        while (token != NULL) {
            gestor.conexiones[i][j] = atoi(token);
            j++;
            token = strtok(NULL, " ");
        }
        i++;
    }
    fclose(fd);
    realizado = inicializarUsuarios();
    return realizado;
}

/*
Función: InicializarUsuarios
Parámetros de Entrada: no tiene
Valor de salida: booleano para saber si los usuarios se inicializaron 
Descripción: inicializa los datos del usuario: seguidores y seguidos y las estadísticas se establecen en 0
*/

bool inicializarUsuarios() {
    bool good = false;
    Cliente cliente;
    Tweet tweet;
    int index = 0;

    estadisticas.usuariosConectados = 0;
    estadisticas.tweetsRecibidos = 0;
    estadisticas.tweetsEnviados = 0;

    for (int i = 0; i < gestor.maxUsers; ++i) {
        cliente.id = i + 1;
        cliente.conectado = false;
        usuarios[i] = cliente;
        for (int j = 0; j < TAMTWEET; ++j) {
            usuarios[i].tweetsRecibidos[j] = tweet;
        }
        limpiarTweets(i+1);
    }

    for (int i = 0; i < gestor.maxUsers; ++i) {
        for (int j = 0; j < gestor.maxUsers; ++j) {
            if (gestor.conexiones[i][j] == 1) {
                usuarios[i].seguidos[j] = 1;
                usuarios[j].seguidores[i] = 1;
            } else {
                usuarios[i].seguidos[j] = 0;
                usuarios[j].seguidores[i] = 0;
            }
        }
    }
    good = true;
    return good;
}

/*
Función: RegistroUsuario
Parámetros de Entrada: Cliente a registrar
Valor de salida: booleano para saber si el cliente se registro
Descripción: hace el registro de un nuevo usuario en el sistema
*/

bool registroUsuario(Cliente cliente) {
    int index;
    bool realizado = false;
    index = encontrarUsuarioDesconectado(cliente.id);
    if (index == -1) {
        mensajeConfirmacion(index, false, cliente, 0);
        return realizado;
    }
    strcpy(usuarios[index].PID, cliente.PID);
    usuarios[index].conectado = true;
    mensajeConfirmacion(index, true, cliente, 0);
    realizado = true;
    return realizado;
}

/*
Función: DesconexionUsuario
Parámetros de Entrada: Cliente que se va a desconectar
Valor de salida: booleano para confirmar desconexión (éxitosa o no) del cliente
Descripción: desconecta a un usuario del sistema siempre y cuando se encuentre conectado
*/

bool desconexionUsuario(Cliente cliente) {
    int index;
    bool realizado = false;
    index = encontrarUsuarioConectado(cliente.id);
    if (index == -1) {
        mensajeConfirmacion(index, false, cliente, 5);
        return realizado;
    }
    usuarios[index].conectado = false;
    estadisticas.usuariosConectados--; //Uso de semaforos?
    mensajeConfirmacion(index, true, cliente, 5);
    realizado = true;
    return realizado;
}

/*
Función: Follow
Parámetros de Entrada: Cliente que va a seguir a otro
Valor de salida: booleano para confirmar el seguimiento (éxitosa o no) del cliente
Descripción: en el arreglo de seguidos del cliente de entrada se le asigna un 1 y
en el arreglo de seguidores del otro cliente se le asigna un 1
*/

bool follow(Cliente cliente) {
    int index;
    bool realizado = false;
    index = encontrarUsuarioConectado(cliente.id);
    if (index == -1 || usuarios[index].seguidos[cliente.idExterno - 1] == 1 || cliente.id == cliente.idExterno) {
        mensajeConfirmacion(index, false, cliente, 1);
        return realizado;
    }
    usuarios[index].seguidos[cliente.idExterno - 1] = 1;
    usuarios[cliente.idExterno - 1].seguidores[index] = 1;
    mensajeConfirmacion(index, true, cliente, 1);
    realizado = true;
    return realizado;
}

/*
Función: Unfollow
Parámetros de Entrada: Cliente que va a dejar a de seguir
Valor de salida: booleano para confirmar el dejar de seguir (éxitoso o no) del cliente
Descripción: en el arreglo de seguidos del cliente de entrada se le asigna un 0 y
en el arreglo de seguidores del otro cliente se le asigna un 0
*/

bool unfollow(Cliente cliente) {
    int index;
    bool realizado = false;
    index = encontrarUsuarioConectado(cliente.id);
    if (index == -1 || usuarios[index].seguidos[cliente.idExterno - 1] == 0) {
        mensajeConfirmacion(index, false, cliente, 2);
        return realizado;
    }
    usuarios[index].seguidos[cliente.idExterno - 1] = 0;
    usuarios[cliente.idExterno - 1].seguidores[index] = 0;
    mensajeConfirmacion(index, true, cliente, 2);
    realizado = true;
    return realizado;
}

/*
Función: LeerTweet
Parámetros de Entrada: Cliente que envía el tweet
Valor de salida: booleano para confirmar la lectura del tweet (éxitosa o no) del cliente
Descripción: envía los tweets a los clientes a los que el cliente tiene como seguidores dependiendo del modo del gestor
*/

bool leerTweet(Cliente cliente) {
    int index, posTweet;
    bool realizado = false;
    index = encontrarUsuarioConectado(cliente.id);
    for (int i = 0; i < gestor.maxUsers; ++i) {
        if (index != -1) {
            if (usuarios[index].seguidores[i] == 1) {
                posTweet = encontrarPosicionTweet(usuarios[i]);
                usuarios[i].tweetsRecibidos[posTweet] = cliente.tweetUsuario;
                usuarios[i].tweetsRecibidos[posTweet].idUsuario = cliente.id;
                if (gestor.modo == 'A') {
                    usuarios[i].tweetsRecibidos[posTweet].modoGestor = true;
                } else
                    usuarios[i].tweetsRecibidos[posTweet].modoGestor = false;
            }
        } else
            return realizado;
    }
    realizado = true;
    return realizado;
}

/*
Función: EncontrarPosicionTweet
Parámetros de Entrada: Cliente que va a recibir el tweet
Valor de salida: entero que contiene la posición del tweet
Descripción: encuentra una posición vacía en el arreglo de tweets recibidos del cliente para colocar un tweet que se vaya a recibir en dicha posición
*/

int encontrarPosicionTweet(Cliente cliente) {
    for (int i = 0; i < TAMTWEET; ++i) {
        if (*(cliente.tweetsRecibidos + i)->tweet == '\0') {
            return i;
        }
    }
    return -1;
}

/*
Función: EnviarTweetAcoplado
Parámetros de Entrada: Cliente que va a enviar el tweet
Valor de salida: booleano para confirmar envío (éxitoso o no) del tweet
Descripción: envía un tweet de un cliente a otro haciendo la apertura de un pipe propio para el envío de dicho tweet
*/

bool enviarTweetAcoplado (Cliente cliente) {
    int index;
    index = encontrarUsuarioConectado(cliente.id);
    for (int i = 0; i < gestor.maxUsers; ++i) {
        if (usuarios[index].seguidores[i] == 1 && usuarios[i].conectado) {
            enviarTweet(usuarios[i]);
            limpiarTweets(encontrarUsuarioConectado(usuarios[i].id));
        }
    }
    estadisticas.tweetsEnviados++; //aumentar la cantidad de tweets enviados en modo acoplado
}

/*
Función: EnviarTweet
Parámetros de Entrada: Cliente que va a recibir el tweet
Valor de salida: booleano para confirmar envío (éxitoso o no) del tweet
Descripción: envia un tweet dependiendo del modo de operación del gestor
*/

bool enviarTweet(Cliente cliente) {
    int index, fdPipeCliente, creado = 0, escritura;
    bool enviado = false;
    index = encontrarUsuarioConectado(cliente.id);
    if (index == -1) {
        return enviado;
    }
    char pipeCliente[NOMPID] = "pipe-", pipeTweet[NOMPID] = "pipeTweet-";
    strcat(pipeCliente, cliente.PID);
    strcat(pipeTweet, cliente.PID);
    do {
        if (gestor.modo == 'A') {
            fdPipeCliente = open(pipeTweet, O_WRONLY);
        } else
            fdPipeCliente = open(pipeCliente, O_WRONLY);
        if (fdPipeCliente == -1) {
            perror("Gestor abriendo pipe de cliente");
            printf("En unos segundos se volvera a intentar...\n");
            sleep(5);
        } else creado = 1;
    } while (creado == 0);
    escritura = write(fdPipeCliente, &usuarios[index], sizeof(Cliente));
    if (escritura == -1) {
        perror("Escritura en pipe de cliente");
    }
    close(fdPipeCliente);
    enviado = true;
    return enviado;
}

/*
Función: LimpiarTweets
Parámetros de Entrada: entero con la posición del cliente en el arreglo de usuarios
Valor de salida: no tiene
Descripción: a un arreglo de carácteres se le establecen todos los espacios como nulos
*/

void limpiarTweets (int index) {
    for (int i = 0; i < TAMTWEET; ++i) {
        memset (usuarios[index].tweetsRecibidos[i].tweet, '\0', TAMTWEET);
        usuarios[index].tweetsRecibidos[i].idUsuario = 0;
    }
}

/*
Función: EncontrarUsuarioDesconectado
Parámetros de Entrada: id del cliente
Valor de salida: entero con el id del cliente existente que está desconectado
Descripción: encuentra el id del cliente que se encuentra desconectado
*/

int encontrarUsuarioDesconectado(int id) {
    for (int i = 0; i < gestor.maxUsers; ++i) {
        if (usuarios[i].id == id && !usuarios[i].conectado) {
            return i;
        }
    }
    return -1;
}

/*
Función: EncontrarUsuarioConectado
Parámetros de Entrada: id del cliente
Valor de salida: entero con el id del cliente existente que está conectado
Descripción: encuentra el id del cliente que se encuentra conectado
*/

int encontrarUsuarioConectado(int id) {
    for (int i = 0; i < gestor.maxUsers; ++i) {
        if (usuarios[i].id == id && usuarios[i].conectado) {
            return i;
        }
    }
    return -1;
}

/*
Función: MensajeConfirmacion
Parámetros de Entrada: entero con el index del usuario que se va a desconectar o que está conectado, booleano para saber si la operación se realizó con éxito o no, Cliente para mostrar parámetros correspondiente al mismo, entero con la opción del switch
Valor de salida: no tiene
Descripción: muestra mensajes de confirmación (éxitosa o no) respecto a las diferentes acciones del programa
*/

void mensajeConfirmacion(int index, bool verificado, Cliente cliente, int opcion) {
    int fdPipeCliente, creado = 0, escritura;
    char pipeCliente[NOMPID] = "pipe-";
    Mensaje confirmacion;

    if (gestor.modo == 'A') {
        confirmacion.modoGestor = true;
    } else
        confirmacion.modoGestor = false;

    strcat(pipeCliente, cliente.PID);
    do {
        fdPipeCliente = open(pipeCliente, O_WRONLY);
        if (fdPipeCliente == -1) {
            perror("Gestor abriendo pipe de cliente");
            printf("En unos segundos se volvera a intentar...\n");
            sleep(5);
        } else creado = 1;
    } while (creado == 0);

    switch (opcion) {
        case 0:
            if (verificado) {
                sprintf(confirmacion.mensaje, "%s", "Se ha registrado correctamente.");
                confirmacion.confirmacion = true;
                printf("El usuario %d con PID %s se encuentra conectado.\n", usuarios[index].id, usuarios[index].PID);
            } else {
                sprintf(confirmacion.mensaje, "%s", "No se ha podido registrar.");
                confirmacion.confirmacion = false;
            }
            break;
        case 1:
            if (verificado) {
                sprintf(confirmacion.mensaje, "%s%d%s", "Empezo a seguir al usuario ", cliente.idExterno,
                        " correctamente.");
                confirmacion.confirmacion = true;
            } else {
                sprintf(confirmacion.mensaje, "%s%d%s", "No ha podido seguir al usuario ", cliente.idExterno, " o ya lo sigue.");
                confirmacion.confirmacion = false;
            }
            break;
        case 2:
            if (verificado) {
                sprintf(confirmacion.mensaje, "%s%d%s", "Dejo de seguir al usuario ", cliente.idExterno,
                        " correctamente.");
                confirmacion.confirmacion = true;
            } else {
                sprintf(confirmacion.mensaje, "%s%d%s", "No ha podido dejar de seguir al usuario ", cliente.idExterno,
                        ".");
                confirmacion.confirmacion = false;
            }
            break;
        case 3:
            if (verificado) {
                sprintf(confirmacion.mensaje, "%s", "Se ha enviado el tweet correctamente a los seguidores.");
                confirmacion.confirmacion = true;
            } else {
                sprintf(confirmacion.mensaje, "%s", "No se ha enviado el tweet correctamente a los seguidores.");
                confirmacion.confirmacion = false;
            }
            break;
        case 5:
            if (verificado) {
                sprintf(confirmacion.mensaje, "%s", "El usuario se ha desconectado del gestor.");
                confirmacion.confirmacion = true;
                printf("El usuario %d con PID %s se ha desconectado.\n", usuarios[index].id, usuarios[index].PID);
            } else {
                sprintf(confirmacion.mensaje, "%s", "El usuario no se ha podido desconectar del gestor.");
                confirmacion.confirmacion = false;
            }
            break;
        default:
            break;
    }
    escritura = write(fdPipeCliente, &confirmacion, sizeof(Mensaje));
    if (escritura == -1) {
        perror("Escritura en pipe de cliente");
    }
    close(fdPipeCliente);
}

/*
Función: MostrarConexiones
Parámetros de Entrada: no tiene
Valor de salida: no tiene
Descripción: muestra los seguidores y seguidos de cada usuario
*/


void mostrarConexiones() {
    for (int i = 0; i < gestor.maxUsers; ++i) {
        printf("\nUsuario: %d:\nSigue a los usuarios:\n   - ", usuarios[i].id);
        for (int j = 0; j < gestor.maxUsers; ++j) {
            if (usuarios[i].seguidos[j] == 1) {
                printf("%d ", usuarios[j].id);
            }
        }
        printf("\nTiene de seguidores a:\n   - ");
        for (int j = 0; j < gestor.maxUsers; ++j) {
            if (usuarios[i].seguidores[j] == 1) {
                printf("%d ", usuarios[j].id);
            }
        }
        printf("\n------------------------");
    }
    printf("\n\n");
}

