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
#define CHAVE_CONTADORES 5678

// Estrutura para armazenar informações de cada aeronave
typedef struct {
    int pid;
    float x, y;
    int pista;
    int lado;
    int status;
    float velocidade;
} Aeronave;

// Estrutura para armazenar contadores de eventos
typedef struct {
    int contReducaoVelocidade;
    int contMudancasPista;
    int contColisoes;
} Contador;

Aeronave* aeronaves;
Contador* contadores;
int ciclo = 0;
int ciclo_iniciado = 0;
int n;
int shmid;
int shcont;

// Função para encerrar o processo de aeronaves
void encerra_aeronaves(int sig) {
    exit(0);
}

// Função para encerrar a simulação e liberar memória compartilhada
void encerrar_simulacao() {
    int totalRed = 0, totalMud = 0, totalCol = 0;
    for (int i = 0; i < n; i++) {
        totalRed += contadores[i].contReducaoVelocidade;
        totalMud += contadores[i].contMudancasPista;
        totalCol += contadores[i].contColisoes;
    }

    printf("\nResumo final:\n");
    printf("Reduções de velocidade: %d\n", totalRed);
    printf("Mudanças de pista: %d\n", totalMud);
    printf("Colisões: %d\n", totalCol);

    shmdt(aeronaves);
    shmdt(contadores);
    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shcont, IPC_RMID, NULL);
    exit(0);
}

// Função para lidar com o sinal de finalização
void sinal_finalizacao(int sig) {
    if (ciclo_iniciado) {
        waitpid(ciclo, NULL, 0);
    }
    printf("\nTodos os avioes pousaram ou foram eliminados.\n");
    encerrar_simulacao();
}

// Função para imprimir o status das aeronaves
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

        printf("Aeronave %c | PID: %d | x: %.2f | y: %.2f | Pista: %d | Lado: %s | Status: %s | Velocidade: %.2f\n",
            'A' + i, aeronaves[i].pid, aeronaves[i].x, aeronaves[i].y,
            aeronaves[i].pista, lado, status, aeronaves[i].velocidade);
    }
    printf("--------------------------------\n");
}

// Função para verificar colisões entre aeronaves
void checar_colisoes(int n) {
    int eliminados[n];
    for (int i = 0; i < n; i++) {
        eliminados[i] = 0;
    }

    for (int i = 0; i < n; i++) {
        if (aeronaves[i].status != 0 || eliminados[i]) { // Se 'i' já colidiu ou não está voando, ignora
            continue;
        }

        for (int j = i + 1; j < n; j++) {
            if (aeronaves[j].status != 0 || eliminados[j]) { // Se  'j' já colidiu ou não está voando, ignora
                continue;
            }

            if (aeronaves[i].lado == aeronaves[j].lado && aeronaves[i].pista == aeronaves[j].pista) { // Verifica se estão na mesma pista
                float dx = fabs(aeronaves[i].x - aeronaves[j].x);
                float dy = fabs(aeronaves[i].y - aeronaves[j].y);

                if (dx < 0.1 && dy < 0.1) {
                    printf("COLISÃO IMINENTE entre %c e %c! Eliminando %c.\n",
                           'A' + i, 'A' + j, 'A' + i);

                    aeronaves[i].status = 2;
                    kill(aeronaves[i].pid, SIGKILL);
                    contadores[i].contColisoes++;
                    eliminados[i] = 1;

                    break;
                } else if (dx < 0.25 && dy < 0.25) {
                    float di = fabs(aeronaves[i].x - 0.5);
                    float dj = fabs(aeronaves[j].x - 0.5);
                    int alvo;

                    if (di < dj) { // Verifica se a aeronave 'i' está mais próxima do centro da pista
                        alvo = j;
                    } else {
                        alvo = i;
                    }

                    kill(aeronaves[alvo].pid, SIGUSR1);
                    kill(aeronaves[alvo].pid, SIGUSR2);
                }
            }
        }
    }
}


// Função para pausar aeronaves
void pausar_aeronaves(int n) {
    for (int i = 0; i < n; i++) {
        if (aeronaves[i].status == 0) {
            kill(aeronaves[i].pid, SIGSTOP);
        }
    }
}

// Função para executar o ciclo de cada aeronave
void ciclo_aeronaves(int n) {
    for (int i = 0; i < n; i++) {
        printf("Aeronave %c: x=%.2f, y=%.2f, velocidade=%.2f\n",
            'A' + i, aeronaves[i].x, aeronaves[i].y, aeronaves[i].velocidade);
        if (aeronaves[i].status == 0) {
            kill(aeronaves[i].pid, SIGCONT);
            sleep(1);
            kill(aeronaves[i].pid, SIGSTOP);
            sleep(1);
        }
    }
}

// Função para executar o ciclo de aeronaves seguindo o algoritmo Round Robin
void ciclo_rr(int n) {
    int contCiclo = 0;
    signal(SIGTERM, encerra_aeronaves);
    while (1) {
        printf("Ciclo %d\n", ++contCiclo);
        ciclo_aeronaves(n);
        checar_colisoes(n);
        int ativos = 0;
        for (int i = 0; i < n; i++) {
            if (aeronaves[i].status == 0) {
                ativos++;
            }
        }
        if (ativos == 0) {
            kill(getppid(), SIGUSR1);
            exit(0);
        }
        sleep(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: ./main <n_avioes>\n");
        return 1;
    }

    n = atoi(argv[1]);
    shmid = shmget(CHAVE_MEM, sizeof(Aeronave) * n, IPC_CREAT | 0666);
    shcont = shmget(CHAVE_CONTADORES, sizeof(Contador) * n, IPC_CREAT | 0666);
    aeronaves = (Aeronave*) shmat(shmid, NULL, 0);
    contadores = (Contador*) shmat(shcont, NULL, 0);

    signal(SIGUSR1, sinal_finalizacao);

    printf("\nComandos:\n1 - Iniciar\n2 - Pausar\n3 - Retomar\n4 - Status\n5 - Finalizar\n");

    char comando;
    while (1) {
        scanf(" %c", &comando);

        if (comando == '1') {
            if (!ciclo_iniciado) {
                ciclo_iniciado = 1;
                for (int i = 0; i < n; i++) {
                    char id[8], total[8];
                    sprintf(id, "%d", i);
                    sprintf(total, "%d", n);
                    if (fork() == 0) {
                        execl("./aviao", "aviao", id, total, NULL);
                        perror("execl");
                        exit(1);
                    }
                }
                sleep(3);
                ciclo = fork();
                if (ciclo == 0) {
                    ciclo_rr(n);
                }
            } else {
                printf("Já iniciado.\n");
            }
        } else if (comando == '2') {
            if (ciclo_iniciado) {
                kill(ciclo, SIGSTOP);
                pausar_aeronaves(n);
                printf("Pausado.\n");
            }
        } else if (comando == '3') {
            if (ciclo_iniciado) {
                kill(ciclo, SIGCONT);
                printf("Retomado.\n");
            }
        } else if (comando == '4') {
            print_status(n);
        } else if (comando == '5') {
            if (ciclo_iniciado) {
                kill(ciclo, SIGTERM);
                waitpid(ciclo, NULL, 0);
            }
            for (int i = 0; i < n; i++) {
                if (aeronaves[i].status == 0) {
                    kill(aeronaves[i].pid, SIGKILL);
                }
            }
            while (wait(NULL) > 0) {
                // espera todos filhos
            }

            printf("\nTodos os avioes pousaram ou foram eliminados.\n");

            encerrar_simulacao();
        }
    }
}
