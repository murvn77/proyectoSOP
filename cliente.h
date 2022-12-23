/*
Archivo: cliente.h
Realizado por: Juan Jaimes y Mauren Rivera - [Camilo Rodríguez]
Contiene: variables para controlar los límites de seguidores, el tamaño máximo del tweet y el nombre del pipe; prototipo de las funciones que maneja el cliente y las estructuras de datos del tweet y del cliente.
Fecha última modificación: 19/11/2022.
*/

#ifndef PROYECTOV3_CLIENTE_H
#define PROYECTOV3_CLIENTE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#define MAXFOLLOW 79
#define TAMTWEET 200
#define NOMPID 20
#define TAMMSJ 60

bool crearPipe();

bool leerRespuesta();

void solicitud(int opcion);

void leerTweetCliente ();

void menu();

void mostrarTweets ();

void crearHilo ();

void mostrarTweetsAcoplado ();

bool crearPipeAcoplado ();

typedef struct Tweet {
    int idUsuario;
    char tweet [TAMTWEET];
    bool modoGestor;
} Tweet;

typedef struct client {
    int id;
    char PID[NOMPID];
    int opcion;
    int seguidores[MAXFOLLOW];
    int seguidos[MAXFOLLOW];
    int idExterno;
    Tweet tweetUsuario;
    Tweet tweetsRecibidos[TAMTWEET];
    bool conectado;
} Cliente;

#endif //PROYECTOV3_CLIENTE_H
