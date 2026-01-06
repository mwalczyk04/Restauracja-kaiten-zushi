#include "common.h"
#include <time.h>

#define SHM_SIZE sizeof(Restauracja)

int main() {
	srand(time(NULL) ^ getpid());

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
		perror("Blad przylaczania do pamieci | kucharz");
		exit(1);
	}


	printf("Kucharz zaczyna prace PID = %d\n", getpid());

	while (1) {

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

		//printf("Przygotowanie dania typu %d\n", typ_dania);
		usleep(100000 + (rand() % 200000)); //Losowy czas przygotowania potrawy

		sem_p(sem_id, SEM_WOLNE);	//Czekanie na miejsce
		sem_p(sem_id, SEM_BLOKADA);	//Blokowanie tasmy

		int pozycja = -1;
		for (int i = 0;i < MAX_TASMA;i++) {
			if (adres->tasma[i].rodzaj == 0) {
				adres->tasma[i].rodzaj = typ_do_ugotowania;
				adres->tasma[i].rezerwacja_dla = dla_kogo;
				adres->tasma[i].cena = 0;
				pozycja = i;
				break;
			}
		}

		sem_v(sem_id, SEM_BLOKADA);	//Odblokowanie tasmy
		sem_v(sem_id, SEM_ZAJETE);	//Informowanie ze danie gotowe

		if (pozycja != -1) {
			if (dla_kogo != 0) {
				printf("[Kucharz] Danie SPECJALNE typy %d polozone na pozycji %d dla %d\n", typ_do_ugotowania, pozycja, dla_kogo);
			}
			else {
				printf("[Kucharz] Danie standardowe typu %d polozone na pozycji %d\n", typ_do_ugotowania, pozycja);
			}
		}
	}





	return 0;
}