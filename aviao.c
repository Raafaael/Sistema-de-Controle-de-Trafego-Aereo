#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <math.h>

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
int aeronave_id;
int total_avioes;
static int velocidadeReduzida = 0;

void reduzVelocidade(int sinal) {
    if (velocidadeReduzida == 0) {
        aeronaves[aeronave_id].velocidade = aeronaves[aeronave_id].velocidade/2;
        velocidadeReduzida = 1;
        printf("Aeronave %c reduziu a velocidade!\n", 'A' + aeronave_id);
    } else {
        aeronaves[aeronave_id].velocidade = 0.05;
        velocidadeReduzida = 0;
        printf("Aeronave %c retornou à velocidade normal!\n", 'A' + aeronave_id);
    }
}

void alterarPista(int sinal) {
    if (aeronaves[aeronave_id].lado == 0) {
        if (aeronaves[aeronave_id].pista == 3) {
            aeronaves[aeronave_id].pista = 18;
            printf("Aeronave %c mudou para a pista 18!\n", 'A' + aeronave_id);
        } else {
            aeronaves[aeronave_id].pista = 3;
            printf("Aeronave %c mudou para a pista 3!\n", 'A' + aeronave_id);
        }
    } else {
        if (aeronaves[aeronave_id].pista == 6) {
            aeronaves[aeronave_id].pista = 27;
            printf("Aeronave %c mudou para a pista 27!\n", 'A' + aeronave_id);
        } else {
            aeronaves[aeronave_id].pista = 6;
            printf("Aeronave %c mudou para a pista 6!\n", 'A' + aeronave_id);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: ./aviao <id> <total_avioes>\n");
        exit(1);
    }

    aeronave_id = atoi(argv[1]);
    total_avioes = atoi(argv[2]);

    int shmid = shmget(CHAVE_MEM, sizeof(Aeronave) * total_avioes, 0666);
    aeronaves = (Aeronave*) shmat(shmid, NULL, 0);

    srand(getpid());
    int atraso = rand() % 3;
    sleep(atraso);
    
    aeronaves[aeronave_id].pid = getpid();

    int lado_sorteado = rand() % 2;
    if (lado_sorteado == 0) {
        aeronaves[aeronave_id].x = 0.0;
        aeronaves[aeronave_id].lado = 0;
    } else {
        aeronaves[aeronave_id].x = 1.0;
        aeronaves[aeronave_id].lado = 1;
    }

    aeronaves[aeronave_id].y = (rand() % 100) / 100.0;
    aeronaves[aeronave_id].velocidade = 0.05;

    int pista_sorteada = rand() % 2;
    if (aeronaves[aeronave_id].lado == 0) {
        if (pista_sorteada == 0) {
            aeronaves[aeronave_id].pista = 3;
        } else {
            aeronaves[aeronave_id].pista = 18;
        }
    } else {
        if (pista_sorteada == 0) {
            aeronaves[aeronave_id].pista = 6;
        } else {
            aeronaves[aeronave_id].pista = 27;
        }
    }

    aeronaves[aeronave_id].status = 0;

    signal(SIGUSR1, reduzVelocidade);
    signal(SIGUSR2, alterarPista);

    kill(getpid(), SIGSTOP);
    printf("Aeronave %c aguardou %d segundos antes de entrar no espaço aéreo...\n", 'A' + aeronave_id, atraso);

    while (1) {
        if (aeronaves[aeronave_id].lado == 0) {
            aeronaves[aeronave_id].x += aeronaves[aeronave_id].velocidade;
        } else {
            aeronaves[aeronave_id].x -= aeronaves[aeronave_id].velocidade;
        }

        if (aeronaves[aeronave_id].y > 0.5) {
            aeronaves[aeronave_id].y -= aeronaves[aeronave_id].velocidade;
            if (aeronaves[aeronave_id].y < 0.5) aeronaves[aeronave_id].y = 0.5;
        } else if (aeronaves[aeronave_id].y < 0.5) {
            aeronaves[aeronave_id].y += aeronaves[aeronave_id].velocidade;
            if (aeronaves[aeronave_id].y > 0.5) aeronaves[aeronave_id].y = 0.5;
        }

        if (fabs(aeronaves[aeronave_id].x - 0.5) < 0.01 && fabs(aeronaves[aeronave_id].y - 0.5) < 0.01) {
            aeronaves[aeronave_id].x = 0.5;
            aeronaves[aeronave_id].y = 0.5;
            aeronaves[aeronave_id].status = 1;
            printf("Aeronave %c pousou.\n", 'A' + aeronave_id);
            exit(0);
        }

        sleep(2);
    }
}
