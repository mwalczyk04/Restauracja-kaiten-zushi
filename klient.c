#include "common.h"
#include <time.h>

#define SHM_SIZE sizeof(Restauracja)

int main() {
	srand(time(NULL));

	key_t klucz = ftok(".", ID_PROJEKT);
	if (klucz == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	int shm_id = shmget(klucz, SHM_SIZE, 0666);
	if (shm_id == -1) {
		perror("Blad podlaczania segmentu pamieci dzielonej!");
		exit(1);
	}

	int sem_id = semget(klucz, 0, 0666);
	if (sem_id == -1) {
		perror("Blad semaforow");
		exit(1);
	}

	Restauracja* adres = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres == (void*)-1) {
		perror("Blad przylaczania do pamieci | klient");
		exit(1);
	}


	printf("Klient o PID = d% przychodzi\n", getpid());

	while (1) {
		sleep(rand() % 4 + 2);

		printf("[Klient] Czekam na jedzenie\n");

		sem_p(sem_id, SEM_ZAJETE);
		sem_p(sem_id, SEM_BLOKADA);

		int pozycja = -1;
		int typ_zjedzony = 0;

		for (int i = 0;i < MAX_TASMA;i++) {
			if (adres->tasma[i].rodzaj != 0) {
				typ_zjedzony = adres->tasma[i].rodzaj;

				//Klient zjadl zerujemy
				adres->tasma[i].rodzaj = 0;
				adres->tasma[i].cena = 0;

				pozycja = i;
				break;
			}
		}


		sem_v(sem_id, SEM_BLOKADA);
		sem_v(sem_id, SEM_WOLNE);

		if (pozycja != -1) {
			printf("[Klient] Zjedzono danie typu %d z pozycji %d\n", typ_zjedzony, pozycja);
		}
		else {
			printf("Blad semafora | klient");
		}


	}


	return 0;
}