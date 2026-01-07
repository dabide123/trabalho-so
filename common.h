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

#define FIFO_SRV "fifo_servidor"
#define FIFO_CLI_FMT "fifo_cli_%d"
#define FIFO_VEI_FMT "fifo_vei_%d"

typedef struct {
    pid_t pid_cliente;
    char username[20];
    char comando[20]; 
    int arg_inteiro;     
    char arg_string[50]; 
    int arg_distancia;   
} PedidoCliente;

typedef struct {
    int codigo; 
    char destino[50];
} CmdVeiculo;

#endif
