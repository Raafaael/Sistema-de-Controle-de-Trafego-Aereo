#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <math.h>

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
    int atraso;
} Contador;

Aeronave* aeronaves;
Contador* contadores;
int aeronave_id;
int total_avioes;
float velocidadeBase = 0.05;

// Função para reduzir a velocidade da aeronave
void reduzVelocidade(int sinal) {
    printf("Aeronave %c reduziu a velocidade\n", 'A' + aeronave_id);
    contadores[aeronave_id].contReducaoVelocidade++;
    aeronaves[aeronave_id].velocidade -= 0.01;
}

// Função para alterar a pista da aeronave
void alterarPista(int sinal) {
    int atual = aeronaves[aeronave_id].pista;

    if (aeronaves[aeronave_id].lado == 0) {
        if (atual == 3) {
            aeronaves[aeronave_id].pista = 18;
        } else {
            aeronaves[aeronave_id].pista = 3;
        }
    } else {
        if (atual == 6) {
            aeronaves[aeronave_id].pista = 27;
        } else {
            aeronaves[aeronave_id].pista = 6;
        }
    }

    if (aeronaves[aeronave_id].pista != atual) {
        contadores[aeronave_id].contMudancasPista++;
        printf("Aeronave %c mudou para pista %d\n", 'A' + aeronave_id, aeronaves[aeronave_id].pista);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        exit(1);
    }

    aeronave_id = atoi(argv[1]);
    total_avioes = atoi(argv[2]);

    int shmid = shmget(CHAVE_MEM, sizeof(Aeronave) * total_avioes, 0666);
    int shcont = shmget(CHAVE_CONTADORES, sizeof(Contador) * total_avioes, 0666);
    aeronaves = (Aeronave*) shmat(shmid, NULL, 0);
    contadores = (Contador*) shmat(shcont, NULL, 0);

    srand(getpid());
    int atraso = rand() % 3;
    sleep(atraso);
    printf("Aeronave %c aguardou %d segundos antes de entrar no espaço aéreo.\n", 'A' + aeronave_id, atraso);

    contadores[aeronave_id].atraso = atraso;

    aeronaves[aeronave_id].pid = getpid();
    aeronaves[aeronave_id].lado = rand() % 2;

    if (aeronaves[aeronave_id].lado == 0) {
        aeronaves[aeronave_id].x = 0.0;
    } else {
        aeronaves[aeronave_id].x = 1.0;
    }

    aeronaves[aeronave_id].y = (rand() % 100) / 100.0;

    velocidadeBase = 0.05 + ((rand() % 100) / 100.0) * 0.02;
    if (velocidadeBase < 0.01) {
        velocidadeBase = 0.05;
    }
    aeronaves[aeronave_id].velocidade = velocidadeBase;

    aeronaves[aeronave_id].status = 0;
    contadores[aeronave_id].contReducaoVelocidade = 0;
    contadores[aeronave_id].contMudancasPista = 0;
    contadores[aeronave_id].contColisoes = 0;

    int sorteio = rand() % 2;
    if (aeronaves[aeronave_id].lado == 0) {
        if (sorteio == 0) {
            aeronaves[aeronave_id].pista = 3;
        } else {
            aeronaves[aeronave_id].pista = 18;
        }
    } else {
        if (sorteio == 0) {
            aeronaves[aeronave_id].pista = 6;
        } else {
            aeronaves[aeronave_id].pista = 27;
        }
    }

    signal(SIGUSR1, reduzVelocidade);
    signal(SIGUSR2, alterarPista);

    kill(getpid(), SIGSTOP);
    sleep(1);

    while (1) {
        float dx = aeronaves[aeronave_id].x - 0.5;
        float dy = aeronaves[aeronave_id].y - 0.5;
        float distancia = dx * dx + dy * dy;
        aeronaves[aeronave_id].velocidade = 0.01 + velocidadeBase * distancia;
    
        if (aeronaves[aeronave_id].lado == 0) {
            aeronaves[aeronave_id].x += aeronaves[aeronave_id].velocidade;
        } else {
            aeronaves[aeronave_id].x -= aeronaves[aeronave_id].velocidade;
        }
    
        if (aeronaves[aeronave_id].y > 0.5) {
            aeronaves[aeronave_id].y -= aeronaves[aeronave_id].velocidade;
            if (aeronaves[aeronave_id].y < 0.5) {
                aeronaves[aeronave_id].y = 0.5;
            }
        } else if (aeronaves[aeronave_id].y < 0.5) {
            aeronaves[aeronave_id].y += aeronaves[aeronave_id].velocidade;
            if (aeronaves[aeronave_id].y > 0.5) {
                aeronaves[aeronave_id].y = 0.5;
            }
        }
    
        if (fabs(aeronaves[aeronave_id].x - 0.5) < 0.01 &&
            fabs(aeronaves[aeronave_id].y - 0.5) < 0.01) {
            aeronaves[aeronave_id].x = 0.5;
            aeronaves[aeronave_id].y = 0.5;
            aeronaves[aeronave_id].status = 1;
            printf("Aeronave %c pousou.\n", 'A' + aeronave_id);
            exit(0);
        }
    
        sleep(1);
    }
}