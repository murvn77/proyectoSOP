/*
Archivo: gestor.h
Realizado por: Juan Jaimes y Mauren Rivera - [Camilo Rodríguez]
Contiene: variable con la cantidad máxima de usuarios y el tamaño del mensaje, prototipo de las funciones que maneja el gestor y las estructuras de datos del mensaje del tweet, del server y de las estadísticas.
Fecha última modificación: 19/11/2022.
*/

#ifndef PROYECTOV3_GESTOR_H
#define PROYECTOV3_GESTOR_H

#define MAXUSERS 80

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include "cliente.h"

Cliente leerPipe();

bool inicializarConexiones();

bool inicializarUsuarios();

bool registroUsuario(Cliente cliente);

int encontrarUsuarioDesconectado(int id);

int encontrarUsuarioConectado(int id);

void mensajeConfirmacion(int index, bool camino, Cliente cliente, int opcion);

void mostrarConexiones();

void imprimirEstadisticas (int idSignal);

bool desconexionUsuario (Cliente cliente);

bool follow (Cliente cliente);

bool unfollow (Cliente cliente);

bool leerTweet (Cliente cliente);

int encontrarPosicionTweet (Cliente cliente);

bool enviarTweetAcoplado (Cliente cliente);

bool enviarTweet (Cliente seguidor);

void limpiarTweets (int index);

void crearAlarma ();

void argumentos (int argc, char **argv);

typedef struct mensaje {
    char mensaje[TAMMSJ];
    bool confirmacion;
    bool modoGestor;
} Mensaje;

typedef struct server {
    int maxUsers;
    char *relaciones;
    char modo;
    int time;
    char *pipeConexion;
    int conexiones[MAXUSERS][MAXUSERS];
} Gestor;

typedef struct estadisticas {
    int usuariosConectados;
    int tweetsEnviados;
    int tweetsRecibidos;
} Estadisticas;


#endif //PROYECTOV3_GESTOR_H
