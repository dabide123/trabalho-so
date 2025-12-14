/* Ficheiro: common.h */
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h> 

// Constantes
#define FIFO_SRV "fifo_servidor"
#define FIFO_CLI_FMT "fifo_cli_%d"
#define FIFO_VEI_FMT "fifo_vei_%d"

// Estruturas
typedef struct {
    pid_t pid_cliente;
    char username[20];
    char comando[20]; // LOGIN, AGENDAR, CONSULTAR, CANCELAR
    // Mudei os nomes para serem genéricos e servirem para vários comandos:
    int arg_inteiro;     // Serve para 'hora' OU 'id_cancelamento'
    char arg_string[50]; // Serve para 'local'
    int arg_distancia;   // Serve para 'distancia'
} PedidoCliente;

typedef struct {
    int codigo; // 1=ENTRAR, 2=SAIR
    char destino[50];
} CmdVeiculo;

#endif
