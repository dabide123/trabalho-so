/* * Ficheiro: cliente.c
 * Função: Interface do utilizador. Usa Threads para I/O simultâneo.
 */
#include "common.h"
#include <pthread.h>

char meu_fifo[50];
char fifo_veiculo_atual[50] = ""; // Guarda o endereço do táxi atribuído
int running = 1;

// --- Thread: Listener ---
// Fica sempre à espera de mensagens no FIFO do cliente
void *listener(void *arg) {
    int fd = open(meu_fifo, O_RDONLY); // Bloqueia até abrir
    char buf[256];
    
    while (running) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            
            // Protocolo: Veículo envia "VEICULO|fifo_do_taxi|mensagem"
            if (strncmp(buf, "VEICULO|", 8) == 0) {
                char *token = strtok(buf, "|"); // Header
                token = strtok(NULL, "|");      // Nome do FIFO do Veículo
                strcpy(fifo_veiculo_atual, token);
                token = strtok(NULL, "|");      // Mensagem de texto
                printf("\n[TÁXI CHEGOU] %s\n> ", token);
            } 
            // Mensagens genéricas
            else if (strncmp(buf, "MSG|", 4) == 0) {
                printf("\n[INFO] %s\n> ", buf+4);
            }
            else {
                printf("\n[SISTEMA] %s\n> ", buf);
            }
            fflush(stdout);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) { printf("Uso: ./cliente <username>\n"); return 1; }

    // Cria FIFO exclusivo (fifo_cli_PID)
    sprintf(meu_fifo, FIFO_CLI_FMT, getpid());
    mkfifo(meu_fifo, 0666);

    // Tenta abrir FIFO do servidor (verifica se controlador está on)
    int fd_srv = open(FIFO_SRV, O_WRONLY);
    if (fd_srv == -1) { 
        printf("Erro: Controlador não está a correr.\n"); 
        unlink(meu_fifo); 
        return 1; 
    }

    // Envia Pedido de Login
    PedidoCliente p;
    p.pid_cliente = getpid();
    strcpy(p.username, argv[1]);
    strcpy(p.comando, "LOGIN");
    write(fd_srv, &p, sizeof(PedidoCliente));

    // Inicia Thread para receber respostas
    pthread_t t;
    if (pthread_create(&t, NULL, listener, NULL) != 0) {
        perror("Erro thread"); return 1;
    }

    printf("Bem-vindo %s. Use: agendar <t> <loc> <dist>, entrar <dest>, sair, terminar\n> ", argv[1]);

    // Loop de Comandos do Utilizador
    char linha[100], cmd[20], arg1[20], arg2[20], arg3[20];
    while (running && fgets(linha, 100, stdin)) {
        int args = sscanf(linha, "%s %s %s %s", cmd, arg1, arg2, arg3);
        if (args < 1) continue;

        if (strcmp(cmd, "agendar") == 0) { // Sintaxe: agendar <hora> <local> <dist>
            strcpy(p.comando, "AGENDAR");
            p.arg_hora = atoi(arg1);
            strcpy(p.arg_local, arg2);
            p.arg_distancia = atoi(arg3);
            write(fd_srv, &p, sizeof(PedidoCliente));
        }
        else if (strcmp(cmd, "entrar") == 0) { // Sintaxe: entrar <destino>
            if (strlen(fifo_veiculo_atual) == 0) {
                printf("Não há veículo à sua espera!\n");
            } else {
                // Comunica DIRETO com o veículo
                int fd_v = open(fifo_veiculo_atual, O_WRONLY);
                if (fd_v != -1) {
                    CmdVeiculo Cv;
                    Cv.codigo = 1; // 1 = ENTRAR
                    strcpy(Cv.destino, arg1);
                    write(fd_v, &Cv, sizeof(CmdVeiculo));
                    close(fd_v);
                    printf("Entraste no táxi. Boa viagem!\n");
                }
            }
        }
        else if (strcmp(cmd, "sair") == 0) { // Sintaxe: sair
             if (strlen(fifo_veiculo_atual) > 0) {
                int fd_v = open(fifo_veiculo_atual, O_WRONLY);
                CmdVeiculo Cv; Cv.codigo = 2; // 2 = SAIR
                write(fd_v, &Cv, sizeof(CmdVeiculo));
                close(fd_v);
                fifo_veiculo_atual[0] = '\0'; // Reset
             }
        }
        else if (strcmp(cmd, "terminar") == 0) {
            running = 0;
            // Opcional: Avisar controlador que saiu
        }
        printf("> ");
    }

    // Limpeza
    pthread_cancel(t);
    close(fd_srv);
    unlink(meu_fifo);
    return 0;
}
