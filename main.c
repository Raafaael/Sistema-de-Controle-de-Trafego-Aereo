#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/select.h>
#include <math.h>
#include <string.h>

#define CHAVE_MEM 1234

typedef struct {
    pid_t pid;
    float x, y;
    int pista;
    int lado;
    int status;
    float velocidade;
} Aeronave;

Aeronave* aeronaves;

int ciclo_ativo = 0;

void print_status(int n) {
    printf("\n----- STATUS DAS AERONAVES -----\n");
    for (int i = 0; i < n; i++) {
        char* lado;
        if (aeronaves[i].lado == 0) {
            lado = "W";
        } else {
            lado = "E";
        }

        char* status;
        if (aeronaves[i].status == 0) {
            status = "VOANDO";
        } else if (aeronaves[i].status == 1) {
            status = "POUSOU";
        } else {
            status = "COLIDIU";
        }

        printf("Aeronave %c | PID: %d | x: %.2f | y: %.2f | Pista: %d | Lado: %s | Status: %s | Velocidade: %f\n",
               'A' + i, aeronaves[i].pid, aeronaves[i].x, aeronaves[i].y,
               aeronaves[i].pista, lado, status, aeronaves[i].velocidade);
    }
    printf("--------------------------------\n");
    //fflush(stdout);
}

void checar_colisoes(int n) {
    for (int i = 0; i < n; i++) {
        if (aeronaves[i].status != 0) continue;

        for (int j = i + 1; j < n; j++) {
            if (aeronaves[j].status != 0) continue;

            if (aeronaves[i].lado == aeronaves[j].lado &&
                aeronaves[i].pista == aeronaves[j].pista) {

                float dx = fabs(aeronaves[i].x - aeronaves[j].x);
                float dy = fabs(aeronaves[i].y - aeronaves[j].y);

                if (dx < 0.1 && dy < 0.1) {
                    printf("COLISÃO IMINENTE entre aeronaves %c e %c na pista %d! Eliminando aeronave %c.\n",
                           'A' + i, 'A' + j, aeronaves[i].pista, 'A' + i);
                    kill(aeronaves[i].pid, SIGKILL);
                    aeronaves[i].status = 2;
                } else if (dx < 0.15 && dy < 0.15) {
                    float dist_i = fabs(aeronaves[i].x - 0.5);
                    float dist_j = fabs(aeronaves[j].x - 0.5);

                    if (dist_i < dist_j) {
                        kill(aeronaves[j].pid, SIGUSR1);
                        kill(aeronaves[j].pid, SIGUSR2);
                    } else {
                        kill(aeronaves[i].pid, SIGUSR1);
                        kill(aeronaves[i].pid, SIGUSR2);
                    }
                }
            }
        }
    }
}

void pausar_aeronaves(int n) {
    for (int i = 0; i < n; i++) {
        if (aeronaves[i].status == 0) {
            kill(aeronaves[i].pid, SIGSTOP);
        }
    }
}

void iniciar_aeronaves(int n) {
    for (int i = 0; i < n; i++) {
        if (aeronaves[i].status == 0) {
            kill(aeronaves[i].pid, SIGCONT);
            sleep(1);
            kill(aeronaves[i].pid, SIGSTOP);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: ./main <n_avioes>\n");
        return 1;
    }

    int n = atoi(argv[1]);

    int shmid = shmget(CHAVE_MEM, sizeof(Aeronave) * n, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("Erro ao criar memória compartilhada");
        return 1;
    }

    aeronaves = (Aeronave*) shmat(shmid, NULL, 0);
    if (aeronaves == (void *) -1) {
        perror("Erro ao mapear memória compartilhada");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        char id_str[8];
        char total_str[8];
        sprintf(id_str, "%d", i);
        sprintf(total_str, "%d", n);
        if (fork() == 0) {
            execl("./aviao", "aviao", id_str, total_str, NULL);
            perror("execl");
            exit(1);
        }
    }

    sleep(1);

    printf("\nComandos disponiveis:\n\n");
    printf("1 - Iniciar aeronaves\n");
    printf("2 - Pausar aeronaves\n");
    printf("3 - Retomar aeronaves\n");
    printf("4 - Status das aeronaves\n");
    printf("5 - Finalizar simulacao\n");

    char comando;
    int start = 0;
    printf("\nDigite um comando: ");
    while(1){
        if(!ciclo_ativo){
            scanf("%c", &comando);
            switch(comando){
                case '1':
                    if(start){
                        printf("Aeronaves ja estao inicializadas\n");
                    }
                    else{
                        ciclo_ativo = 1;
                        start = 1;
                        iniciar_aeronaves(n);
                        printf("Aeronaves inicializadas\n");
                    }
                    break;
                case '2':
                    printf("Aeronaves nao estao ativas\n");
                    break;
                case '3':
                    if(!start){
                        printf("Aeronaves nao foram inicializadas\n");
                    }
                    else{
                        ciclo_ativo = 1;
                        printf("Voo das aeronaves retomado\n");
                        iniciar_aeronaves(n);
                    }
                    break;
                case '4':
                    print_status(n);
                    break;
                case '5':
                    for(int i = 0; i < n; i++){
                        if(aeronaves[i].status == 0){
                            kill(aeronaves[i].pid, SIGKILL);
                        }
                    }
                    while(wait(NULL) > 0);
                    shmdt(aeronaves);
                    shmctl(shmid, IPC_RMID, NULL);
                    printf("Simulacao finalizada\n");
                    return 0;
                default:
                    printf("\nDigite um comando: ");
            }   
        }
        if(ciclo_ativo){
            iniciar_aeronaves(n);
            checar_colisoes(n);

            fd_set read_terminal;
            struct timeval timeout;
            FD_ZERO(&read_terminal);
            FD_SET(STDIN_FILENO, &read_terminal);
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

            int ready = select(STDIN_FILENO + 1, &read_terminal, NULL, NULL, &timeout);
            if(ready > 0 && FD_ISSET(STDIN_FILENO, &read_terminal)){
                scanf("%c", &comando);
                switch(comando){
                    case '2':
                        ciclo_ativo = 0;
                        pausar_aeronaves;
                        printf("Aeronaves pausadas\n");
                        break;
                    case '4':
                        print_status(n);
                        break;
                    case '5':
                        for(int i = 0; i < n; i++){
                            if(aeronaves[i].status == 0){
                                kill(aeronaves[i].pid, SIGKILL);
                            }
                        }
                        while(wait(NULL) > 0);
                        shmdt(aeronaves);
                        shmctl(shmid, IPC_RMID, NULL);
                        printf("Simulacao finalizada\n");
                        return 0;
                    default:
                        ("\nDigite um comando: ");
                }
            }
            int ativos = 0;
            for (int i = 0; i < n; i++) {
                if (aeronaves[i].status == 0) {
                    ativos++;
                }
            }
            if(ativos == 0){
                printf("\nTodos os avioes pousaram ou foram eliminados\n");
                shmdt(aeronaves);
                shmctl(shmid, IPC_RMID, NULL);
                return 0;
            } 
        }
    }
}