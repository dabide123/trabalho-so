/* * Ficheiro: veiculo.c
 * Função: Simula deslocação e interage com Cliente (direto) e Controlador (stdout).
 */
#include "common.h"

int fd_cli = -1;
int fd_meu_fifo = -1;
char meu_fifo_nome[50];

// Handler para limpar recursos antes de sair
void cleanup(int s) {
    if (s == SIGUSR1) printf("CANCELADO\n"); // Mensagem para o Controlador ler
    if (fd_cli != -1) close(fd_cli);
    if (fd_meu_fifo != -1) close(fd_meu_fifo);
    unlink(meu_fifo_nome); // Apaga o FIFO próprio
    exit(0);
}

int main(int argc, char *argv[]) {
    // Argumentos recebidos do execl no controlador:
    // [1]=ID_Servico, [2]=Distancia, [3]=PID_Cliente
    if (argc < 4) return 1;

    int distancia = atoi(argv[2]);
    pid_t pid_cliente = atoi(argv[3]);

    // Regista handlers de sinais
    signal(SIGUSR1, cleanup); // Cancelamento pelo controlador
    signal(SIGINT, SIG_IGN);  // Ignora Ctrl+C (só o controlador gere o shutdown)

    // 1. Criar FIFO exclusivo deste veículo para receber comandos do cliente (ENTRAR/SAIR)
    sprintf(meu_fifo_nome, FIFO_VEI_FMT, getpid());
    mkfifo(meu_fifo_nome, 0666);

    // 2. Contactar o Cliente para dizer "Cheguei"
    // Usa o FIFO do cliente cujo formato é conhecido (fifo_cli_PID)
    char fifo_cli_nome[50];
    sprintf(fifo_cli_nome, FIFO_CLI_FMT, pid_cliente);
    
    fd_cli = open(fifo_cli_nome, O_WRONLY);
    if (fd_cli == -1) cleanup(0); // Cliente desapareceu?

    // Envia mensagem especial com o nome do FIFO deste veículo
    char msg_inicial[100];
    sprintf(msg_inicial, "VEICULO|%s|Estou no local. Digite 'entrar <destino>'.", meu_fifo_nome);
    write(fd_cli, msg_inicial, strlen(msg_inicial)+1);
    
    // 3. Aguardar comando 'ENTRAR'
    int fd_leitura = open(meu_fifo_nome, O_RDONLY);
    CmdVeiculo cmd;
    
    // Bloqueia aqui até o cliente escrever no FIFO do veículo
    read(fd_leitura, &cmd, sizeof(CmdVeiculo));
    
    if (cmd.codigo != 1) cleanup(0); // Se não for ENTRAR, aborta

    // Avisa Controlador que a viagem começou (via STDOUT -> Pipe Anónimo)
    printf("PERC:0\n"); 
    fflush(stdout); // CRÍTICO: Força o envio imediato, sem buffer!

    // 4. Simulação da Viagem
    int percorrido = 0;
    
    // Torna a leitura do FIFO do veículo não bloqueante para verificar 'SAIR' durante o loop
    int flags = fcntl(fd_leitura, F_GETFL, 0);
    fcntl(fd_leitura, F_SETFL, flags | O_NONBLOCK);

    while (percorrido < distancia) {
        sleep(1); // Simulação: 1 segundo = 1 km [cite: 170]
        percorrido++;
        
        // Reporta a cada 10%
        int perc = (percorrido * 100) / distancia;
        if (perc % 10 == 0) {
            printf("PERC:%d\n", perc); // Telemetria
            fflush(stdout);
        }

        // Verifica se cliente quer SAIR a meio
        if (read(fd_leitura, &cmd, sizeof(CmdVeiculo)) > 0) {
            if (cmd.codigo == 2) { // Código 2 = SAIR
                printf("FIM:%d\n", percorrido); // Informa KMs reais
                fflush(stdout);
                cleanup(0);
            }
        }
    }

    // 5. Fim da Viagem
    printf("FIM:%d\n", distancia);
    fflush(stdout);

    // Envia mensagem final ao cliente
    char msg_fim[] = "MSG|Chegámos ao destino. Obrigado.";
    write(fd_cli, msg_fim, strlen(msg_fim)+1);

    cleanup(0);
    return 0;
}
