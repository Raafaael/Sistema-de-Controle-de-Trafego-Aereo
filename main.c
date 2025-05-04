#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
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
pid_t ciclo = 0;
int ciclo_iniciado = 0;

void encerra_aeronaves(int sig){
    exit(0);
}

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

void ciclo_aeronaves(int n) {
    for (int i = 0; i < n; i++) {
        if (aeronaves[i].status == 0) {
            kill(aeronaves[i].pid, SIGCONT);
            sleep(1);
            kill(aeronaves[i].pid, SIGSTOP);
        }
    }
}

void ciclo_rr(int n){
    signal(SIGTERM, encerra_aeronaves);
    while(1){
        ciclo_aeronaves(n);
        checar_colisoes(n);
        sleep(1);
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
    printf("\nDigite um comando: ");
    while(1){
        scanf("%c", &comando);

        switch(comando){
            case '1':
                if (ciclo_iniciado){
                    printf("Aeronaves ja foram iniciadas, use 3 para retomar\n");
                }
                else{
                    ciclo_iniciado = 1;
                    printf("Aeronaves inicializadas\n");
                    ciclo = fork();
                    if(ciclo == 0){
                        ciclo_rr(n);
                    }
                    else if(ciclo < 0){
                        perror("Erro ao iniciar aeronaves\n");
                        return 1;
                    }
                }
                break;
            case '2':
                if(!ciclo_iniciado){
                    printf("Aeronaves nao foram iniciadas\n");
                }
                else{
                    kill(ciclo, SIGSTOP);
                    pausar_aeronaves(n);
                    printf("Aeronaves pausadas\n");
                }
                break;
            case '3':
                if(!ciclo_iniciado){
                    printf("Aeronaves nao inicializadas, utilize 1 para iniciar\n");
                }
                else{
                    kill(ciclo, SIGCONT);
                    printf("Aeronaves retomadas\n");
                }
                break;
            case '4':
                print_status(n);
                break;
            case '5':
                if(ciclo_iniciado){
                    kill(ciclo, SIGTERM);
                    waitpid(ciclo, NULL, 0);
                }
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

        int ativos = 0;
        for (int i = 0; i < n; i++) {
            if (aeronaves[i].status == 0) {
                ativos++;
            }
        }
        if (ativos == 0) {
            if(ciclo_iniciado){
                kill(ciclo, SIGTERM);
                waitpid(ciclo, NULL, 0);
            }
            printf("\nTodos os avioes pousaram ou foram eliminados.\n");
            shmdt(aeronaves);
            shmctl(shmid, IPC_RMID, NULL);
            return 0;
        }
    }
}