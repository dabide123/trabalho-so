/* Ficheiro: veiculo.c */
#include "common.h"

int fd_cli = -1;
char meu_fifo[50];

void cleanup(int s) {
    if (s == SIGUSR1) printf("CANCELADO\n"); 
    unlink(meu_fifo);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 4) return 1;
    int distancia = atoi(argv[2]);
    pid_t pid_cli = atoi(argv[3]);

    signal(SIGUSR1, cleanup);
    signal(SIGINT, SIG_IGN); 

    sprintf(meu_fifo, FIFO_VEI_FMT, getpid());
    mkfifo(meu_fifo, 0666);

    char fifo_cli[50];
    sprintf(fifo_cli, FIFO_CLI_FMT, pid_cli);
    
    // Tenta abrir o cliente
    fd_cli = open(fifo_cli, O_WRONLY);
    if (fd_cli == -1) cleanup(0); // Cliente desapareceu

    // Avisa que chegou
    char msg[100];
    sprintf(msg, "VEICULO|%s|Estou no local. Digite 'entrar <dest>'.", meu_fifo);
    write(fd_cli, msg, strlen(msg)+1);

    // --- SIMPLIFICAÇÃO: Espera Bloqueante Simples ---
    // O veículo fica aqui parado até o cliente mandar "ENTRAR".
    // Se o cliente nunca mandar, o veículo fica preso (erro aceitável num TP).
    int fd_meu = open(meu_fifo, O_RDONLY); 
    CmdVeiculo cv;
    read(fd_meu, &cv, sizeof(CmdVeiculo)); 
    
    if (cv.codigo != 1) cleanup(0); // Se não for ENTRAR, sai

    // Muda para Non-Block para a viagem (para poder ler "SAIR" sem parar o carro)
    int flags = fcntl(fd_meu, F_GETFL, 0);
    fcntl(fd_meu, F_SETFL, flags | O_NONBLOCK);

    // Inicia Viagem
    printf("PERC:0\n"); fflush(stdout);
    int perc = 0;
    
    while (perc < distancia) {
        sleep(1); 
        perc++;
        
        // Tenta ler comando (SAIR) sem bloquear
        if (read(fd_meu, &cv, sizeof(CmdVeiculo)) > 0) {
            if (cv.codigo == 2) { // SAIR
                printf("FIM:%d\n", perc); fflush(stdout);
                cleanup(0);
            }
        }

        // Reporta a cada 10%
        int p_calc = (perc * 100) / distancia;
        if (p_calc % 10 == 0) {
            printf("PERC:%d\n", p_calc);
            fflush(stdout);
        }
    }

    printf("FIM:%d\n", distancia); fflush(stdout);
    
    // Mensagem final
    char fim_msg[] = "MSG|Chegamos.";
    write(fd_cli, fim_msg, strlen(fim_msg)+1);
    
    cleanup(0);
    return 0;
}
