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
	if (adres_globalny == NULL)_exit(0);
	if (adres_globalny->czy_ewakuacja == 1) {
		printf("\n[Obsluga] Otrzymalem sygnal SIGTERM.Ewakuacja\n\n");
	}
	else {
		printf("[Obsluga] Koniec zmiany\n");
	}

	printf("=======================================\n");
	printf("[Obsluga] RAPORT FINANSOWY:\n");
	printf(" Utarg: %d zl\n", adres_globalny->utarg);
	printf(" Zmarnowane jedzienie:\n");
	int strata = 0;
	int znalezione = 0;

	for (int i = 0;i < MAX_TASMA;i++) {
		int typ = adres_globalny->tasma[i].rodzaj;

		if (typ != 0) {
			int cena = pobierz_cene(typ);
			printf(" - Pozycja %2d: Danie typu %d o wartosci %d zl\n", i, typ, cena);
			strata += cena;
			znalezione = 1;
		}

	}
	if (!znalezione) {
		printf("Tasma pusta , brak strat\n");
	}
	else {
		printf("LACZNA WARTOSC STRAT: %d zl\n", strata);
	}
	printf("=======================================\n");

	
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

	int shm_id = shmget(klucz_shm, SHM_SIZE, 0600);
	if (shm_id == -1) {
		perror("Blad podlaczania segmentu pamieci dzielonej!");
		exit(1);
	}

	int msg_id = msgget(klucz_msg, IPC_CREAT | 0600);
	if (msg_id == -1) {
		perror("Blad msgget");
		exit(1);
	}

	Restauracja* adres = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres == (void*)-1) {
		perror("Blad przylaczania do pamieci | obsluga");
		exit(1);
	}
	adres_globalny = adres;

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