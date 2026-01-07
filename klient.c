#include "common.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>

#define SHM_SIZE sizeof(Restauracja)

int main() {
	srand(time(NULL) ^ getpid());

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

	int sem_id = semget(klucz_shm, 0, 0666);
	if (sem_id == -1) {
		perror("Blad semaforow");
		exit(1);
	}

	int msg_id = msgget(klucz_msg, 0666);
	if (msg_id == -1) {
		perror("Blad msgget");
		exit(1);
	}

	Restauracja* adres = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres == (void*)-1) {
		perror("Blad przylaczania do pamieci | klient");
		exit(1);
	}

	KomunikatZaplaty msg;

	
	

	int czy_vip = 0;
	if (rand() % 100 < 2) {
		czy_vip = 1;
	}


	sem_p(sem_id, SEM_BLOKADA);

	if (adres->czy_otwarte == 0) {
		sem_v(sem_id, SEM_BLOKADA);	//Jesli zamkniete to odchodzi
		return 0;
	}

	adres->liczba_klientow++;
	sem_v(sem_id, SEM_BLOKADA);;


	if (czy_vip) {
		printf("[VIP %d] Przychodzi\n", getpid());
	}
	else {
		printf("GRUPA o PID = %d przychodzi\n", getpid());
	}
	fflush(stdout);

		sleep(rand() % 6 + 2);

		//Generowanie grupy klientow
		int liczba_osob = 1 + (rand() % 4);
		int dorosli = 1 + (rand() % liczba_osob);
		int dzieci = liczba_osob - dorosli;
		int ile_zjedza = 3 + (rand() % 8);

		int semafor_docelowy = -1;
		int ilosc_do_zajecia = 0;

		if (czy_vip) {

			int znaleziono = 0;

			int wolne_4 = semctl(sem_id, SEM_STOL_4, GETVAL);
			int wolne_3 = semctl(sem_id, SEM_STOL_3, GETVAL);
			int wolne_2 = semctl(sem_id, SEM_STOL_2, GETVAL);
			int wolne_1 = semctl(sem_id, SEM_STOL_1, GETVAL);

			//Najpierw szukamy pasujacego albo wiekszego stolika zeby VIP nie czekal
			
			if (liczba_osob == 1) {
				if (wolne_1 >= 1) { semafor_docelowy = SEM_STOL_1; ilosc_do_zajecia = 1; znaleziono = 1; }
				else if (wolne_2 >= 2) { semafor_docelowy = SEM_STOL_2; ilosc_do_zajecia = 2; znaleziono = 1; }
				else if (wolne_3 >= 3) { semafor_docelowy = SEM_STOL_3; ilosc_do_zajecia = 3; znaleziono = 1; }
				else if (wolne_4 >= 4) { semafor_docelowy = SEM_STOL_4; ilosc_do_zajecia = 4; znaleziono = 1; }
			}
			else if (liczba_osob == 2) {
				if (wolne_2 >= 2) { semafor_docelowy = SEM_STOL_2; ilosc_do_zajecia = 2; znaleziono = 1; }
				else if (wolne_3 >= 3) { semafor_docelowy = SEM_STOL_3; ilosc_do_zajecia = 3; znaleziono = 1; }
				else if (wolne_4 >= 4) { semafor_docelowy = SEM_STOL_4; ilosc_do_zajecia = 4; znaleziono = 1; }
			}
			else if (liczba_osob == 3) {
				if (wolne_3 >= 3) { semafor_docelowy = SEM_STOL_3; ilosc_do_zajecia = 3; znaleziono = 1; }
				else if (wolne_4 >= 4) { semafor_docelowy = SEM_STOL_4; ilosc_do_zajecia = 4; znaleziono = 1; }
			}
			else {
				if (wolne_4 >= 4) { semafor_docelowy = SEM_STOL_4; ilosc_do_zajecia = 4; znaleziono = 1; }
			}

			if (znaleziono) {
				printf("[VIP %d] Zajmuje stolik %d os. Siada bez kolejki\n", getpid(), ilosc_do_zajecia);
			}
			else {
				printf("[VIP %d] Wszystko zajete, VIP musi czekac\n", getpid());
				if (liczba_osob == 1) { semafor_docelowy = SEM_STOL_1; ilosc_do_zajecia = 1; }
				else if (liczba_osob == 2) { semafor_docelowy = SEM_STOL_2; ilosc_do_zajecia = 2; }
				else if (liczba_osob == 3) { semafor_docelowy = SEM_STOL_3; ilosc_do_zajecia = 3; }
				else { semafor_docelowy = SEM_STOL_4; ilosc_do_zajecia = 4; }
			}


		}
		else {



			int czy_lada = 0;

			if (dzieci == 0 && liczba_osob <= 2) {
				if (rand() % 2 == 0) czy_lada = 1;	//50% szans na lade dla grup do 2 osob
			}


			if (czy_lada) {
				semafor_docelowy = SEM_LADA;
				ilosc_do_zajecia = liczba_osob;
				printf("[GRUPA %d](%d os) Zajmuje miejsce przy ladzie\n", getpid(), liczba_osob);
			}
			else {
				switch (liczba_osob)
				{
				case 1:
					if (rand() % 2 == 0) {
						semafor_docelowy = SEM_STOL_1;
						ilosc_do_zajecia = 1;
						printf("[GRUPA %d](1 os) Stolik 1 os(Zajmuje caly)\n", getpid());
					}
					else {
						semafor_docelowy = SEM_STOL_2;
						ilosc_do_zajecia = 1;
						printf("[GRUPA %d](1 os) Stolik 2 os (Dosiada sie)\n", getpid());
					}
					break;
				case 2:
					if (rand() % 2 == 0) {
						semafor_docelowy = SEM_STOL_2;
						ilosc_do_zajecia = 2;
						printf("[GRUPA %d](2 os) Stolik 2 os (Zajmuje caly)\n", getpid());
					}
					else {
						semafor_docelowy = SEM_STOL_4;
						ilosc_do_zajecia = 2;
						printf("[GRUPA %d](2 os) Stolik 4 os (Dosiada sie)\n", getpid());
					}
					break;
				case 3:
					semafor_docelowy = SEM_STOL_3;
					ilosc_do_zajecia = 3;
					printf("[GRUPA %d](3 os) Stolik 3 os (Zajmuje caly)\n", getpid());
					break;
				default:
					semafor_docelowy = SEM_STOL_4;
					ilosc_do_zajecia = 4;
					printf("[GRUPA %d](4 os) Stolik 4 os (Zajmuje caly)\n", getpid());
					break;
				}

			}
		}
		sem_zmiana(sem_id, semafor_docelowy, -ilosc_do_zajecia);	//Zajecie miejsca

		int rachunek_grupy = 0;

		for (int k = 0;k < ile_zjedza;k++) {
		
			if (adres->czy_ewakuacja == 1 || adres->czy_otwarte == 0)break;
		//int pozycja = -1;
		int czy_specjal = 0;
		int typ_zamowiony = 0;
		if (rand() % 100 < 30) {
			typ_zamowiony = 4 + (rand() % 3);
			sem_p(sem_id, SEM_BLOKADA);

			for (int i = 0;i < MAX_ZAMOWIEN;i++) {
				if (adres->tablet[i].pid_klienta == 0) {
					adres->tablet[i].pid_klienta = getpid();
					adres->tablet[i].typ_dania = typ_zamowiony;
					czy_specjal = 1;
					printf("[GRUPA %d] Zamawiam w tablecie danie %d\n", getpid(), typ_zamowiony);
					break;
				}
			}
			sem_v(sem_id, SEM_BLOKADA);
		}

		int zjedzone = 0;
		int typ_zjedzony = 0;
		int rezerwacja = 0;
		int do_zaplaty = 0;

		while (!zjedzone) {
		
			sem_p(sem_id, SEM_ZAJETE);
			sem_p(sem_id, SEM_BLOKADA);

			for (int i = 0;i < MAX_TASMA;i++) {
				if (adres->tasma[i].rodzaj != 0) {
					typ_zjedzony = adres->tasma[i].rodzaj;
					rezerwacja = adres->tasma[i].rezerwacja_dla;
					do_zaplaty = 0;

					if (typ_zjedzony == 1) { do_zaplaty = CENA_DANIA_1; }
					else if (typ_zjedzony == 2) { do_zaplaty = CENA_DANIA_2; }
					else if (typ_zjedzony == 3) { do_zaplaty = CENA_DANIA_3; }
					else if (typ_zjedzony == 4) { do_zaplaty = CENA_DANIA_4; }
					else if (typ_zjedzony == 5) { do_zaplaty = CENA_DANIA_5; }
					else if (typ_zjedzony == 6) { do_zaplaty = CENA_DANIA_6; }

					if (czy_specjal) {
						if (rezerwacja == getpid()) {
							rachunek_grupy += do_zaplaty;
							adres->tasma[i].rodzaj = 0;
							adres->tasma[i].rezerwacja_dla = 0;
							zjedzone = 1;
							printf("[GRUPA %d] Otrzymano zamowienie SPECJALNE %d\n", getpid(), typ_zjedzony);
							break;
						}
					}
					else {
						if (rezerwacja == 0) {
							rachunek_grupy += do_zaplaty;
							adres->tasma[i].rodzaj = 0;
							zjedzone = 1;
							break;
						}
					}
				}
			}
			
			sem_v(sem_id, SEM_BLOKADA);

			if(zjedzone){
				sem_v(sem_id, SEM_WOLNE);
			}
			else {
				sem_v(sem_id, SEM_ZAJETE);	//Oddaje semafor jak nic nie wzial
				usleep(50000);
			}

		}
		usleep(1000000 + (rand() % 500000));

		}
		//adres->utarg += rachunek_grupy;
		//printf("[GRUPA %d] Zaplacono %d zl, Wychodzimy\n",getpid(),rachunek_grupy);
		msg.mtype = 1;
		msg.pid_klienta = getpid();
		msg.kwota = rachunek_grupy;

		if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
			perror("Blad wyslania zaplaty");
		}
		else {
			printf("[GRUPA %d] Wyslano do kasy %d zl, Wychodzimy\n",getpid(),rachunek_grupy);
		}

		sem_zmiana(sem_id, semafor_docelowy, ilosc_do_zajecia);	//Zwolnienie miejsca

		sem_p(sem_id, SEM_BLOKADA);
		adres->liczba_klientow--;
		sem_v(sem_id, SEM_BLOKADA);


	return 0;
}