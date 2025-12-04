/* * Ficheiro: controlador.c
 * Função: Gere a frota, tempo simulado e pedidos. NÃO usa select().
 */
#include "common.h"

#define MAX_CLI 30
#define MAX_VEI_DEFAULT 10
#define MAX_SERVICOS 100

// --- Estruturas Internas do Controlador ---
typedef struct {
    pid_t pid;
    int fd_leitura; // O descritor do pipe anónimo para ler o stdout deste veículo
    int ocupado;    // 1 se estiver em missão
    int id_servico; // ID do serviço que está a realizar
    int percentagem;// Estado atual (0-100)
} Veiculo;

typedef struct {
    int id;
    int hora;
    char local[50];
    int distancia;
    pid_t pid_cliente;
    char username[20];
    int estado;     // 0=Pendente, 1=Em Curso, 2=Concluido
    int veiculo_idx;// Qual o índice do veículo que pegou o serviço
} Servico;

typedef struct {
    char username[20];
    pid_t pid;
    int ativo;
} Utilizador;

// --- Variáveis Globais ---
Veiculo *frota;
int n_veiculos = 0;
Servico servicos[MAX_SERVICOS];
Utilizador utilizadores[MAX_CLI];
int tempo_simulado = 0;
int running = 1;

// --- Função: set_nonblock ---
// Configura um descritor de ficheiro para não bloquear o processo se não houver dados.
// Essencial para substituir o 'select'.
void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// --- Função: termina_sistema ---
// Limpeza graciosa ao receber SIGINT ou comando 'terminar'
void termina_sistema() {
    printf("\n[CONTROLADOR] A encerrar sistema e a recolher frota...\n");
    for (int i = 0; i < n_veiculos; i++) {
        if (frota[i].pid > 0 && frota[i].ocupado) {
            // Envia SIGUSR1 para cancelar veículos ativos [cite: 84]
            kill(frota[i].pid, SIGUSR1);
        }
    }
    unlink(FIFO_SRV); // Apaga o pipe do disco
    free(frota);
    exit(0);
}

// --- Função: lança_veiculo ---
// Faz fork, configura pipe anónimo e executa o programa 'veiculo'
void lança_veiculo(int slot_frota, int slot_servico) {
    int p[2];
    if (pipe(p) == -1) return; // Erro ao criar pipe anónimo

    pid_t pid = fork();
    
    if (pid == 0) { // --- PROCESSO FILHO (Veículo) ---
        close(p[0]); // Fecha leitura (o filho só escreve)
        
        // Redireciona o STDOUT para o pipe (Telemetria vai por aqui) [cite: 41]
        dup2(p[1], STDOUT_FILENO); 
        close(p[1]);

        // Prepara argumentos strings para passar ao exec [cite: 38]
        char arg_id[10], arg_dist[10], arg_cli_pid[20];
        sprintf(arg_id, "%d", servicos[slot_servico].id);
        sprintf(arg_dist, "%d", servicos[slot_servico].distancia);
        sprintf(arg_cli_pid, "%d", servicos[slot_servico].pid_cliente);

        execl("./veiculo", "veiculo", arg_id, arg_dist, arg_cli_pid, NULL);
        exit(1); // Só chega aqui se o execl falhar
    } 
    else { // --- PROCESSO PAI (Controlador) ---
        close(p[1]); // Fecha escrita (o pai só lê)
        set_nonblock(p[0]); // Leitura não bloqueante para o polling loop
        
        // Regista o veículo na estrutura
        frota[slot_frota].pid = pid;
        frota[slot_frota].fd_leitura = p[0];
        frota[slot_frota].ocupado = 1;
        frota[slot_frota].id_servico = servicos[slot_servico].id;
        frota[slot_frota].percentagem = 0;
        
        servicos[slot_servico].estado = 1; // Marca serviço como 'Em Curso'
        servicos[slot_servico].veiculo_idx = slot_frota;
    }
}

int main() {
    // Configuração Inicial
    char *env = getenv("NVEICULOS");
    n_veiculos = env ? atoi(env) : MAX_VEI_DEFAULT;
    frota = calloc(n_veiculos, sizeof(Veiculo));

    // Criação do Named Pipe Principal
    if (mkfifo(FIFO_SRV, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo"); return 1;
    }

    signal(SIGINT, termina_sistema);

    // Abre FIFO em modo Non-Blocking
    int fd_srv = open(FIFO_SRV, O_RDONLY | O_NONBLOCK);
    // Truque: Às vezes é preciso reabrir para garantir handle correto em alguns sistemas UNIX
    if (fd_srv == -1) fd_srv = open(FIFO_SRV, O_RDONLY | O_NONBLOCK);

    set_nonblock(STDIN_FILENO); // Permite ler comandos do admin sem bloquear

    printf("[CONTROLADOR] Iniciado (Frota: %d). Aguardando...\n", n_veiculos);
    time_t last_tick = time(NULL);

    // --- LOOP PRINCIPAL (POLLING) ---
    while (running) {
        
        // 1. Gestão de Tempo (1 seg real = 1 unidade tempo simulado)
        if (time(NULL) > last_tick) {
            tempo_simulado++;
            last_tick = time(NULL);
            
            // Verificar se há serviços agendados para "agora"
            for (int i = 0; i < MAX_SERVICOS; i++) {
                if (servicos[i].id != 0 && servicos[i].estado == 0 && servicos[i].hora <= tempo_simulado) {
                    // Procura veículo livre
                    for (int v = 0; v < n_veiculos; v++) {
                        if (!frota[v].ocupado) {
                            lança_veiculo(v, i);
                            printf("[AGENDAMENTO] Serviço %d iniciado pelo Veículo %d\n", servicos[i].id, frota[v].pid);
                            break;
                        }
                    }
                }
            }
        }

        // 2. Ler Comandos Admin (STDIN)
        char buf[100];
        if (read(STDIN_FILENO, buf, sizeof(buf)-1) > 0) {
            strtok(buf, "\n"); // Remove newline
            if (strncmp(buf, "listar", 6) == 0) {
                printf("--- Serviços ---\n");
                for(int i=0; i<MAX_SERVICOS; i++) if(servicos[i].id) 
                    printf("ID:%d | User:%s | Estado:%d\n", servicos[i].id, servicos[i].username, servicos[i].estado);
            } 
            else if (strncmp(buf, "terminar", 8) == 0) {
                termina_sistema(0);
            }
            // Adicionar aqui outros comandos: 'frota', 'utiliz'
        }

        // 3. Ler Pedidos dos Clientes (FIFO Named Pipe)
        PedidoCliente p;
        if (read(fd_srv, &p, sizeof(PedidoCliente)) == sizeof(PedidoCliente)) {
            if (strcmp(p.comando, "LOGIN") == 0) {
                // Registar utilizador (simplificado)
                for(int i=0; i<MAX_CLI; i++) if(!utilizadores[i].ativo) {
                    utilizadores[i].ativo = 1;
                    utilizadores[i].pid = p.pid_cliente;
                    strcpy(utilizadores[i].username, p.username);
                    printf("[LOGIN] %s entrou.\n", p.username);
                    break;
                }
            } 
            else if (strcmp(p.comando, "AGENDAR") == 0) {
                // Encontrar slot livre para serviço
                for(int i=0; i<MAX_SERVICOS; i++) if(servicos[i].id == 0) {
                    servicos[i].id = i + 1;
                    servicos[i].hora = p.arg_hora;
                    servicos[i].distancia = p.arg_distancia;
                    strcpy(servicos[i].local, p.arg_local);
                    servicos[i].pid_cliente = p.pid_cliente;
                    strcpy(servicos[i].username, p.username);
                    servicos[i].estado = 0; // Pendente
                    printf("[REQ] Serviço agendado (ID %d) para T=%d\n", servicos[i].id, p.arg_hora);
                    break;
                }
            }
            // Adicionar lógica CANCELAR aqui
        }

        // 4. Ler Telemetria dos Veículos (Pipes Anónimos - STDOUT dos filhos)
        char vbuf[50];
        for (int v = 0; v < n_veiculos; v++) {
            if (frota[v].ocupado) {
                int n = read(frota[v].fd_leitura, vbuf, sizeof(vbuf)-1);
                
                if (n > 0) {
                    vbuf[n] = '\0';
                    // O veículo envia strings como "PERC:10" ou "FIM:5"
                    if (strncmp(vbuf, "PERC:", 5) == 0) {
                        frota[v].percentagem = atoi(vbuf+5);
                    } 
                    else if (strncmp(vbuf, "FIM:", 4) == 0) {
                        printf("[FIM] Serviço %d concluído.\n", frota[v].id_servico);
                        
                        // Limpeza do processo filho
                        close(frota[v].fd_leitura);
                        frota[v].ocupado = 0;
                        waitpid(frota[v].pid, NULL, 0); // Evita zombies
                    }
                }
            }
        }

        usleep(10000); // Pausa de 10ms para não sobrecarregar o CPU (Polling)
    }
    return 0;
}
