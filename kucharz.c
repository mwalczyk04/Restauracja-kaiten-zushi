#include "common.h"
#include <time.h>
#include <signal.h>
#include <stdlib.h>

#define SHM_SIZE sizeof(Restauracja)

//volatile aby kompilator wiedzial ze zmienna zmienia sie nagle 
volatile int opoznienie_bazowe = 200000;

//Zmienne do raportu koncowego
int wyprodukowane[7] = { 0 };
int laczna_wartosc = 0;

int pobierz_cene(int typ) {
	if (typ == 1)return CENA_DANIA_1;
	if (typ == 2)return CENA_DANIA_2;
	if (typ == 3)return CENA_DANIA_3;
	if (typ == 4)return CENA_DANIA_4;
	if (typ == 5)return CENA_DANIA_5;
	if (typ == 6)return CENA_DANIA_6;
	return 0;
}

Restauracja* adres_globalny = NULL;

void obsluga_sygnalow(int sig) {
	if (sig == SIGUSR1) {
		//Przyspieszenie
		opoznienie_bazowe = opoznienie_bazowe / 2;

		if (opoznienie_bazowe < 20000) opoznienie_bazowe = 20000;	//Zabezpieczenie dla kucharza

		char msg[] = "\n[Kucharz] Otrzymalem sygnal SIGUSR1 przyspieszam (2x)\n\n";
		write(1, msg, sizeof(msg) - 1);
	}
	else if (sig == SIGUSR2) {
		//Zwolnienie
		opoznienie_bazowe = opoznienie_bazowe * 2;

		char msg[] = "\n[Kucharz] Otrzymalem sygnal SIGUSR2 zwalniam (0.5x)\n\n";
		write(1, msg, sizeof(msg) - 1);
	}
	else if (sig == SIGTERM) {

		if (adres_globalny != NULL && adres_globalny->czy_ewakuacja == 1) {
			printf("\n[Kucharz] Otrzymalem sygnal SIGTERM. Ewakuacja\n\n");
		}
		else {
			printf("[Kucharz] Koniec zmiany\n");
		}

		printf("=======================================\n");
		printf("[Kucharz] RAPORT PRODUKCJI:\n");
		for (int i = 0;i <= 6;i++) {
			if (wyprodukowane[i] > 0) {
				printf("- Danie typ %d, %d sztuk, cena za sztuke %d zl\n", i, wyprodukowane[i], pobierz_cene(i));
			}
			
		}
		printf(" LACZNA WARTOSC: %d zl\n", laczna_wartosc);
		printf("=======================================\n");
		_exit(0);	//Natychamiastowe wyjscie
	}
}

int main() {
	signal(SIGINT, SIG_IGN);	//Ignorowanie Ctrl+C
	signal(SIGUSR1, obsluga_sygnalow);
	signal(SIGUSR2, obsluga_sygnalow);
	signal(SIGTERM, obsluga_sygnalow);

	srand(time(NULL) ^ getpid());

	key_t klucz = ftok(".", ID_PROJEKT);
	if (klucz == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	int shm_id = custom_error(shmget(klucz, SHM_SIZE, 0600), "Blad shmget | kucharz");

	int sem_id = custom_error(semget(klucz, 0, 0600), "Blad semget | kucharz");

	Restauracja* adres = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres == (void*)-1) {
		perror("Blad przylaczania do pamieci | kucharz");
		exit(1);
	}
	adres_globalny = adres;


	printf("Kucharz zaczyna prace PID = %d\n", getpid());

	while (1) {
		if (adres->czy_ewakuacja)break;

		int typ_do_ugotowania = 0;
		int dla_kogo = 0;
		int cena_dania = 0;

		sem_p(sem_id, SEM_BLOKADA);

		for (int i = 0;i < MAX_ZAMOWIEN;i++) {
			if (adres->tablet[i].pid_klienta != 0) {
				typ_do_ugotowania = adres->tablet[i].typ_dania;
				dla_kogo = adres->tablet[i].pid_klienta;

				adres->tablet[i].pid_klienta = 0;
				adres->tablet[i].typ_dania = 0;

				printf("[Kucharz] Przyjal zamowienie SPECJALNE (typ %d) dla %d\n", typ_do_ugotowania, dla_kogo);
				break;
			}
		}
		
		sem_v(sem_id, SEM_BLOKADA);

		if (typ_do_ugotowania == 0) {

			int wolne_miejsce = semctl(sem_id, SEM_WOLNE, GETVAL);	//Pobieramy wartosc semafora 

			if (wolne_miejsce < 3) {
				usleep(200000);
				continue;	//Czeka chwile i wraca na poczatek petli while
			}

			typ_do_ugotowania = (rand() % 3) + 1;
			dla_kogo = 0;
		}

		usleep(opoznienie_bazowe + (rand() % 50000)); //Losowy czas przygotowania potrawy

		int czy_polozono = 0;

		while (!czy_polozono) {
			if (adres->czy_ewakuacja)break;

			sem_p(sem_id, SEM_BLOKADA);

			if (adres->tasma[0].rodzaj == 0) {
				adres->tasma[0].rodzaj = typ_do_ugotowania;
				adres->tasma[0].rezerwacja_dla = dla_kogo;
				adres->tasma[0].cena = 0;
				
				wyprodukowane[typ_do_ugotowania]++;
				laczna_wartosc += pobierz_cene(typ_do_ugotowania);

				sem_p(sem_id, SEM_WOLNE);

				czy_polozono = 1;
			
				if (dla_kogo != 0) {
					printf("[Kucharz] Danie SPECJALNE typy %d polozone na tasmie dla %d\n", typ_do_ugotowania, dla_kogo);
				}
				else {
					printf("[Kucharz] Danie standardowe typu %d polozone na tasmie\n", typ_do_ugotowania);
				}
			}
			sem_v(sem_id, SEM_BLOKADA);

			if (!czy_polozono) {
				//Jesli nie udalo sie polozyc to czeka
				usleep(50000);
			}
		}
	}





	return 0;
}