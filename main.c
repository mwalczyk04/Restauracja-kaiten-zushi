#include "common.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define SHM_SIZE sizeof(Restauracja)

int shm_id = -1;
Restauracja * adres_restauracji = NULL;

void sprzatanie(int sig) {
	
	printf("\nOtrzymano sygnal %d\n Zaczynam sprzatac\n", sig);

	if (adres_restauracji != NULL) {
		if (shmdt(adres_restauracji) == -1) {
			perror("Blad odlaczenia pamieci dzielonej");
		}
		else {
			printf("Pamiec dzielona odlaczona\n");
		}
	}

	if (shm_id != -1) {
		if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
			perror("Blad usuwania pamieci dzielonej");
		}
		else {
			printf("Usunieta pamiec dzielona\n");
		}
	}

	// Dodac tu pozniej usuwanie semaforow
	exit(0);

}

int main() {
	signal(SIGINT, sprzatanie);// ctrl+c == sprzatanie

	key_t klucz = ftok(".", ID_PROJEKT);
	if (klucz == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	shm_id = shmget(klucz, SHM_SIZE, IPC_CREAT | 0666);
	if (shm_id == -1) {
		perror("Blad podlaczania segmentu pamieci dzielonej!");
		exit(1);
	}
	adres_restauracji = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres_restauracji == (void*)-1) {
		perror("Blad przylaczenia (shmat)");
		shmctl(shm_id, IPC_RMID, NULL);
		exit(1);
	}

	adres_restauracji->czy_otwarte = 1;
	adres_restauracji->utarg = 0;

	for (int i = 0;i < MAX_TASMA;i++) {
		adres_restauracji->tasma[i].rodzaj = 0;
	}
	
	printf("Restauracja otwarta ID pamieci = %d\n", shm_id);
	while (1) {
		sleep(1);
	}

	return 0;
}
