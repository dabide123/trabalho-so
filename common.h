/* * Ficheiro: common.h
 * Descrição: Estruturas e constantes partilhadas entre Cliente, Veículo e Controlador.
 */
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

// --- Constantes de Comunicação ---
#define FIFO_SRV "fifo_servidor"      // Onde o Controlador ouve pedidos
#define FIFO_CLI_FMT "fifo_cli_%d"    // Template para o FIFO de receção do Cliente (usa PID)
#define FIFO_VEI_FMT "fifo_vei_%d"    // Template para o FIFO de receção do Veículo (usa PID)

// --- Estruturas de Dados ---

// 1. Pedido enviado do Cliente para o Controlador (via FIFO_SRV)
typedef struct {
    pid_t pid_cliente;
    char username[20];
    char comando[20]; // Ex: "AGENDAR", "LOGIN", "TERMINAR"
    // Argumentos opcionais dependendo do comando
    int arg_hora;
    char arg_local[50];
    int arg_distancia;
} PedidoCliente;

// 2. Comando direto do Cliente para o Veículo (via FIFO do Veículo)
typedef struct {
    int codigo;       // 1 = ENTRAR, 2 = SAIR
    char destino[50]; // Apenas informativo
} CmdVeiculo;

#endif
