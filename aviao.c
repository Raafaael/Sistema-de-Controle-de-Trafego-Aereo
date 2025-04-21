#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#define MAX_AVIOES 20
#define VELOCIDADE_BASE 0.05
#define CHAVE_MEM 1234

typedef struct {
    pid_t pid;
    float x, y;
    int pista;      // 3, 6, 18, 27
    int lado;       // 0: W, 1: E
    int status;     // 0: ativo, 1: pousou, 2: colidiu
    float velocidade;
} Aeronave;

Aeronave* aeronaves;

int main(int argc, char *argv[]) {
    int aeronave_id = atoi(argv[1]);
    int shmid = shmget(CHAVE_MEM, sizeof(Aeronave) * MAX_AVIOES, 0666);
    aeronaves = (Aeronave*) shmat(shmid, NULL, 0);

    srand(getpid());
    int atraso = rand() % 3;
    sleep(atraso);
    printf("Aeronave %c aguardou %d segundos antes de entrar no espaço aéreo...\n", 'A' + aeronave_id, atraso);

    aeronaves[aeronave_id].pid = getpid();

    int lado_sorteado = rand() % 2;
    if (lado_sorteado == 0) {
        aeronaves[aeronave_id].x = 0.0;
        aeronaves[aeronave_id].lado = 0; // W
    } else {
        aeronaves[aeronave_id].x = 1.0;
        aeronaves[aeronave_id].lado = 1; // E
    }

    aeronaves[aeronave_id].y = (rand() % 100) / 100.0; // y entre 0 e 1
    aeronaves[aeronave_id].velocidade = VELOCIDADE_BASE;

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

    while (1) {
        if (aeronaves[aeronave_id].lado == 0) {
            aeronaves[aeronave_id].x += aeronaves[aeronave_id].velocidade;
        } else {
            aeronaves[aeronave_id].x -= aeronaves[aeronave_id].velocidade;
        }

        if ((aeronaves[aeronave_id].lado == 0 && aeronaves[aeronave_id].x >= 0.5) ||
            (aeronaves[aeronave_id].lado == 1 && aeronaves[aeronave_id].x <= 0.5)) {
            aeronaves[aeronave_id].status = 1;
            printf("Aeronave %c pousou.\n", 'A'+aeronave_id);
            exit(0);
        }

        sleep(2);
    }
}
