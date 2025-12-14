/* Ficheiro: cliente.c */
#include "common.h"
#include <pthread.h>

char meu_fifo[50];
char fifo_veiculo[50] = "";
int running = 1;

void *listener(void *arg) {
    (void)arg;
    int fd = open(meu_fifo, O_RDONLY);
    char buf[512];
    
    while (running) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            // Simplificação: Não limpa a linha atual, o texto pode encavalar no prompt
            if (strncmp(buf, "VEICULO|", 8) == 0) {
                char *tk = strtok(buf, "|"); 
                tk = strtok(NULL, "|");
                strcpy(fifo_veiculo, tk);
                tk = strtok(NULL, "|");
                printf("\n[TAXI] %s\n> ", tk);
            } else if (strncmp(buf, "MSG|", 4) == 0) {
                printf("\n[INFO] %s\n> ", buf+4);
            }
            fflush(stdout);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) { printf("Uso: ./cliente <username>\n"); return 1; }

    sprintf(meu_fifo, FIFO_CLI_FMT, getpid());
    mkfifo(meu_fifo, 0666);

    int fd_srv = open(FIFO_SRV, O_WRONLY);
    if (fd_srv == -1) { printf("Controlador off.\n"); unlink(meu_fifo); return 1; }

    PedidoCliente p;
    p.pid_cliente = getpid();
    strcpy(p.username, argv[1]);
    strcpy(p.comando, "LOGIN");
    write(fd_srv, &p, sizeof(PedidoCliente));

    pthread_t t;
    pthread_create(&t, NULL, listener, NULL);

    printf("Bem-vindo. Use: agendar, entrar, sair, terminar\n> ");

    char linha[100], cmd[20], a1[20], a2[20], a3[20];
    while (running && fgets(linha, 100, stdin)) {
        // Parsing simplificado - pode falhar se input for estranho
        sscanf(linha, "%s %s %s %s", cmd, a1, a2, a3);

        if (strcmp(cmd, "agendar") == 0) {
            strcpy(p.comando, "AGENDAR");
            p.arg_inteiro = atoi(a1);
            strcpy(p.arg_string, a2);
            p.arg_distancia = atoi(a3);
            write(fd_srv, &p, sizeof(PedidoCliente));
        }
        else if (strcmp(cmd, "entrar") == 0) {
            if (strlen(fifo_veiculo) > 0) {
                int fd_v = open(fifo_veiculo, O_WRONLY);
                CmdVeiculo cv; cv.codigo=1; 
                write(fd_v, &cv, sizeof(CmdVeiculo));
                close(fd_v);
            } else printf("Sem veiculo.\n");
        }
        else if (strcmp(cmd, "sair") == 0) {
            if (strlen(fifo_veiculo) > 0) {
                int fd_v = open(fifo_veiculo, O_WRONLY);
                CmdVeiculo cv; cv.codigo=2;
                write(fd_v, &cv, sizeof(CmdVeiculo));
                close(fd_v);
            }
        }
        else if (strcmp(cmd, "terminar") == 0) {
            running = 0;
        }
        // Faltam comandos como consultar/cancelar (funcionalidade "em falta")
        printf("> ");
    }
    
    close(fd_srv);
    unlink(meu_fifo);
    return 0;
}
