#include "common.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define SHM_SIZE sizeof(Restauracja)

int main() {
	key_t klucz_shm = ftok(".", ID_PROJEKT);
	key_t klucz_msg = ftok(".", ID_KOLEJKA);
	if (klucz_shm == -1 || klucz_msg == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	int shm_id = shmget(klucz_shm, SHM_SIZE, 0666);
	if (shm_id == -1) {
		perror("Blad podlaczania segmentu pamieci dzielonej!");
		exit(1);
	}

	int msg_id = msgget(klucz_msg, IPC_CREAT | 0666);
	if (msg_id == -1) {
		perror("Blad msgget");
		exit(1);
	}

	Restauracja* adres = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres == (void*)-1) {
		perror("Blad przylaczania do pamieci | obsluga");
		exit(1);
	}

	printf("[Obsluga] Kasjer gotowy. Czekanie na platnosci\n");

	KomunikatZaplaty msg;

	while (1) {
		//Odbieranie komunikatu typu 1
		int wynik = msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 1, 0);
		
		if (wynik != -1) {
			printf("[Obsluga] Klient %d placi rachunek: %d zl\n", msg.pid_klienta, msg.kwota);
			adres->utarg += msg.kwota;
		}
		else {
			if (errno == EIDRM || errno == EINVAL) {
				printf("[Obsluga] Kolejka usunieta - zamykanie procesu\n");
				break;
			}
			else {
				perror("Blad msgcrv");
				break;
			}
		}
	}

	shmdt(adres);
	return 0;
}