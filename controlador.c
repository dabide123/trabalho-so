#include "common.h"

#define MAX_CLI 30
#define MAX_SERVICOS 100

typedef struct {
    pid_t pid;
    int fd_leitura; 
    int ocupado;    
    int id_servico; 
    int percentagem;
} Veiculo;

typedef struct {
    int id;
    int hora;
    char local[50];
    int distancia;
    pid_t pid_cliente;
    char username[20];
    int estado;     
} Servico;

typedef struct {
    char username[20];
    pid_t pid;
    int ativo;
} Utilizador;

Veiculo *frota;
int n_veiculos = 0;
Servico servicos[MAX_SERVICOS];
Utilizador utilizadores[MAX_CLI];
int tempo_simulado = 0;
int running = 1;

void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void avisa_cliente(pid_t pid, char *msg) {
    char fifo_name[50];
    sprintf(fifo_name, FIFO_CLI_FMT, pid);
    int fd = open(fifo_name, O_WRONLY | O_NONBLOCK);
    if (fd != -1) {
        char formatado[256];
        sprintf(formatado, "MSG|%s", msg);
        write(fd, formatado, strlen(formatado)+1);
        close(fd);
    }
}

void termina_sistema(int s) {
    (void)s;
    printf("\n[CONTROLADOR] A encerrar...\n");
    for (int i = 0; i < n_veiculos; i++) {
        if (frota[i].pid > 0 && frota[i].ocupado) {
            kill(frota[i].pid, SIGUSR1); 
        }
    }
    unlink(FIFO_SRV);
    free(frota);
    exit(0);
}

void lanca_veiculo(int slot_frota, int slot_servico) {
    int p[2];
    if (pipe(p) == -1) return;

    pid_t pid = fork();
    if (pid == 0) { 
        close(p[0]);
        dup2(p[1], STDOUT_FILENO); 
        close(p[1]);
        
        char arg_id[10], arg_dist[10], arg_cli[20];
        sprintf(arg_id, "%d", servicos[slot_servico].id);
        sprintf(arg_dist, "%d", servicos[slot_servico].distancia);
        sprintf(arg_cli, "%d", servicos[slot_servico].pid_cliente);
        
        execl("./veiculo", "veiculo", arg_id, arg_dist, arg_cli, NULL);
        exit(1);
    } else { 
        close(p[1]);
        set_nonblock(p[0]);
        frota[slot_frota].pid = pid;
        frota[slot_frota].fd_leitura = p[0];
        frota[slot_frota].ocupado = 1;
        frota[slot_frota].id_servico = servicos[slot_servico].id;
        frota[slot_frota].percentagem = 0;
        
        servicos[slot_servico].estado = 1;
    }
}

int main() {
    char *env = getenv("NVEICULOS"); 
    n_veiculos = env ? atoi(env) : 10; 
    frota = calloc(n_veiculos, sizeof(Veiculo));

    if (mkfifo(FIFO_SRV, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo"); return 1;
    }

    signal(SIGINT, termina_sistema);

    int fd_srv = open(FIFO_SRV, O_RDONLY | O_NONBLOCK);
    int keep_alive_fd = open(FIFO_SRV, O_WRONLY); 
    (void)keep_alive_fd;

    set_nonblock(STDIN_FILENO);
    printf("[CONTROLADOR] Iniciado (NVEICULOS=%d).\n", n_veiculos);
    
    time_t last_tick = time(NULL);

    while (running) {
        if (time(NULL) > last_tick) {
            tempo_simulado++; 
            last_tick = time(NULL);
            
            for (int i = 0; i < MAX_SERVICOS; i++) {
                if (servicos[i].id != 0 && servicos[i].estado == 0 && servicos[i].hora <= tempo_simulado) {
                    for (int v = 0; v < n_veiculos; v++) {
                        if (!frota[v].ocupado) {
                            lanca_veiculo(v, i);
                            printf("[INFO] Táxi %d enviado para o serviço %d (Cliente: %s)\n", v, servicos[i].id, servicos[i].username);
                            break; 
                        }
                    }
                }
            }
        }

        char cmd_buf[50];
        if (read(STDIN_FILENO, cmd_buf, sizeof(cmd_buf)-1) > 0) {
            strtok(cmd_buf, "\n");
            if (strcmp(cmd_buf, "listar") == 0) {
                for(int i=0; i<MAX_SERVICOS; i++) 
                    if(servicos[i].id != 0) printf("ID:%d User:%s Est:%d\n", servicos[i].id, servicos[i].username, servicos[i].estado);
            }
            else if (strcmp(cmd_buf, "terminar") == 0) termina_sistema(0);
        }

        PedidoCliente p;
        if (read(fd_srv, &p, sizeof(PedidoCliente)) == sizeof(PedidoCliente)) {
            if (strcmp(p.comando, "LOGIN") == 0) {
                for(int i=0; i<MAX_CLI; i++) if(!utilizadores[i].ativo) {
                    utilizadores[i].ativo = 1; utilizadores[i].pid = p.pid_cliente;
                    strcpy(utilizadores[i].username, p.username);
                    avisa_cliente(p.pid_cliente, "Login OK");
                    printf("User %s entrou.\n", p.username);
                    break;
                }
            }
            else if (strcmp(p.comando, "AGENDAR") == 0) {
                for(int i=0; i<MAX_SERVICOS; i++) if(servicos[i].id == 0) {
                    servicos[i].id = i+1; servicos[i].hora = p.arg_inteiro;
                    servicos[i].distancia = p.arg_distancia; servicos[i].pid_cliente = p.pid_cliente;
                    strcpy(servicos[i].username, p.username);
                    servicos[i].estado = 0;
                    char m[50]; sprintf(m, "Agendado ID %d", servicos[i].id);
                    avisa_cliente(p.pid_cliente, m);
                    break;
                }
            }
        }

        char vbuf[50];
        for (int v = 0; v < n_veiculos; v++) {
            if (frota[v].ocupado) {
                int n = read(frota[v].fd_leitura, vbuf, sizeof(vbuf)-1);
                if (n > 0) {
                    vbuf[n] = '\0';
                    if (strncmp(vbuf, "PERC:", 5) == 0) {
                        frota[v].percentagem = atoi(vbuf + 5);
                        printf("[INFO] Veiculo %d: %d%% da viagem.\n", v, frota[v].percentagem);
                    }
                    else if (strncmp(vbuf, "FIM:", 4) == 0) {
                        printf("[FIM] Servico %d acabou.\n", frota[v].id_servico);
                        
                        for(int s=0; s<MAX_SERVICOS; s++) 
                            if(servicos[s].id == frota[v].id_servico) servicos[s].estado = 2;

                        close(frota[v].fd_leitura);
                        frota[v].ocupado = 0;
                        waitpid(frota[v].pid, NULL, 0); 
                    }
                }
            }
        }
        usleep(10000); 
    }
    return 0;
}
