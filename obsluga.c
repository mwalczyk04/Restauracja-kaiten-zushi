#include "common.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>

#define SHM_SIZE sizeof(Restauracja)

Restauracja* adres_globalny = NULL;

int pobierz_cene(int typ) {
	if (typ == 1)return CENA_DANIA_1;
	if (typ == 2)return CENA_DANIA_2;
	if (typ == 3)return CENA_DANIA_3;
	if (typ == 4)return CENA_DANIA_4;
	if (typ == 5)return CENA_DANIA_5;
	if (typ == 6)return CENA_DANIA_6;
	return 0;
}

void ewakuacja(int sig) {
	usleep(200000);

	key_t klucz = ftok(".", ID_PROJEKT);
	if (klucz == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}
	int sem_id = custom_error(semget(klucz, 0, 0600), "Blad semget | obsluga");
	if(sem_id != -1) sem_p(sem_id, SEM_BLOKADA);
	if (adres_globalny == NULL)_exit(0);
	if (adres_globalny->czy_ewakuacja == 1) {
		printf(K_GREEN"\n[Obsluga] Otrzymalem sygnal SIGTERM.Ewakuacja\n\n"K_RESET);
	}
	else {
		printf("[Obsluga] Koniec zmiany\n");
	}

	printf(K_GREEN"=======================================\n"K_RESET);
	printf(K_GREEN"[Obsluga] RAPORT FINANSOWY:\n"K_RESET);
	printf(K_GREEN" Utarg: %d zl\n"K_RESET, adres_globalny->utarg);
	printf(K_GREEN" Zmarnowane jedzienie:\n"K_RESET);
	int strata = 0;
	int znalezione = 0;

	for (int i = 0;i < MAX_TASMA;i++) {
		int typ = adres_globalny->tasma[i].rodzaj;

		if (typ != 0) {
			int cena = pobierz_cene(typ);
			printf(K_GREEN" - Pozycja %2d: Danie typu %d o wartosci %d zl\n"K_RESET, i, typ, cena);
			strata += cena;
			znalezione = 1;
		}

	}
	if (!znalezione) {
		printf(K_GREEN"Tasma pusta , brak strat\n"K_RESET);
	}
	else {
		printf(K_GREEN"LACZNA WARTOSC STRAT: %d zl\n"K_RESET, strata);
	}
	printf(K_GREEN"=======================================\n"K_RESET);

	if (sem_id != -1) sem_v(sem_id, SEM_BLOKADA);
	_exit(0);	//Natychamiastowe wyjscie
}

int main() {
	signal(SIGINT, SIG_IGN);	//Ignorowanie Ctrl+C
	signal(SIGTERM, ewakuacja);

	key_t klucz_shm = ftok(".", ID_PROJEKT);
	key_t klucz_msg = ftok(".", ID_KOLEJKA);
	if (klucz_shm == -1 || klucz_msg == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	int shm_id = custom_error(shmget(klucz_shm, SHM_SIZE, 0600), "Blad shmget | obsluga");
	
	int msg_id = custom_error(msgget(klucz_msg, IPC_CREAT | 0600), "Blad msgget | obsluga");

	Restauracja* adres = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres == (void*)-1) {
		perror("Blad przylaczania do pamieci | obsluga");
		exit(1);
	}
	adres_globalny = adres;

	printf(K_GREEN"[Obsluga] Kasjer gotowy. Czekanie na platnosci\n"K_RESET);

	KomunikatZaplaty msg;

	while (1) {
		//Odbieranie komunikatu typu 1
		int wynik = msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 1, 0);
		
		if (wynik != -1) {
			printf(K_GREEN"[Obsluga] Klient %d placi rachunek: %d zl\n"K_RESET, msg.pid_klienta, msg.kwota);
			adres->utarg += msg.kwota;
		}
		else {
			if (errno == EIDRM || errno == EINVAL) {
				printf(K_GREEN"[Obsluga] Kolejka usunieta - zamykanie procesu\n"K_RESET);
				break;
			}
			//else {
				//perror("Blad msgcrv");
				//break;
			//}
		}
	}

	shmdt(adres);
	return 0;
}