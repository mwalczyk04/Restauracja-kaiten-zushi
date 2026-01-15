#include "common.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#define SHM_SIZE sizeof(Restauracja)

Restauracja* adres;	//Zmienna globalna dla watkow
int sem_id;
int pid_grupy;
int czy_vip = 0;

//Globalny numer zeby watki wiedzialy gdzie siedza
// 0 = lada  1+ numer stolika
int numer_stolika_globalny = 0;

int rachunek_grupy = 0;
pthread_mutex_t mutex_rachunek = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	int nr_osoby;
	int czy_dziecko;//0 = dorosly  1 = dziecko
}DaneOsoby;


void* zachowanie_osoby(void* arg) {
	DaneOsoby* dane_osoby = (DaneOsoby*)arg;
	int nr = dane_osoby->nr_osoby;
	int dziecko = dane_osoby->czy_dziecko;


	int ile_zjedza = 3 + (rand() % 8);

	for (int k = 0;k < ile_zjedza;k++) {

		if (adres->czy_ewakuacja == 1)break;

		int czy_specjal = 0;
		int typ_zamowiony = 0;

		if (rand() % 100 < 30) {
			typ_zamowiony = 4 + (rand() % 3);
			sem_p(sem_id, SEM_BLOKADA);

			for (int i = 0;i < MAX_ZAMOWIEN;i++) {
				if (adres->tablet[i].pid_klienta == 0) {
					adres->tablet[i].pid_klienta = pid_grupy;
					adres->tablet[i].typ_dania = typ_zamowiony;
					czy_specjal = 1;
					printf("[%s %d] Zamawiam w tablecie danie %d\n", czy_vip ? "VIP" : "GRUPA", getpid(), typ_zamowiony);
					break;
				}
			}
			sem_v(sem_id, SEM_BLOKADA);
		}
		else {
			//Zamowienie standardowe
			typ_zamowiony = 1 + (rand() % 3);
		}
		//Jesli brak miejsca w tablecie na specjal klkient zamawia standardowe
		if (czy_specjal == 0 && typ_zamowiony > 3)typ_zamowiony = 1 + (rand() % 3);

		int zjedzone = 0;
		int cierpliwosc = 0;
		
		while (!zjedzone) {

			if (adres->czy_ewakuacja == 1) break;

			int strefa_lady = ILOSC_MIEJSC_LADA;
			int moj_id_tasmy;

			if (numer_stolika_globalny > 0) {
				//Stolik patrzy na tasme po ladzie
				moj_id_tasmy = (numer_stolika_globalny - 1) + strefa_lady;
			}
			else {
				//Losowe miejscie przy ladzie
				moj_id_tasmy = rand() % strefa_lady;
			}

			if (moj_id_tasmy >= MAX_TASMA) {
				//Zapetlenie zeby nie wyjsc poza zakres
				moj_id_tasmy = moj_id_tasmy % MAX_TASMA;
			}

			sem_p(sem_id, SEM_BLOKADA);

			int typ_zjedzony = adres->tasma[moj_id_tasmy].rodzaj;
			int rezerwacja = adres->tasma[moj_id_tasmy].rezerwacja_dla;
			int moje_zamowienie = 0;

			if (czy_specjal && rezerwacja == pid_grupy && typ_zjedzony == typ_zamowiony) moje_zamowienie = 1;
			if (!czy_specjal && rezerwacja == 0 && typ_zjedzony == typ_zamowiony) moje_zamowienie = 1;

			cierpliwosc++;
			int wez_cokolwiek = 0;

			//Jesli czeka kilka sekund bierze cokolwiek
			if (cierpliwosc > 300) {
				if (typ_zjedzony != 0 && rezerwacja == 0 && typ_zjedzony <= 3) {
					wez_cokolwiek = 1;
				}
			}

			//Warunek ratunkowy jakby nie bylo na tasmie tego co klient chce
			if (wez_cokolwiek && rezerwacja == 0) moje_zamowienie = 1;


			if (moje_zamowienie) {

				int do_zaplaty = 0;
				if (typ_zjedzony == 1) { do_zaplaty = CENA_DANIA_1; }
				else if (typ_zjedzony == 2) { do_zaplaty = CENA_DANIA_2; }
				else if (typ_zjedzony == 3) { do_zaplaty = CENA_DANIA_3; }
				else if (typ_zjedzony == 4) { do_zaplaty = CENA_DANIA_4; }
				else if (typ_zjedzony == 5) { do_zaplaty = CENA_DANIA_5; }
				else if (typ_zjedzony == 6) { do_zaplaty = CENA_DANIA_6; }

				pthread_mutex_lock(&mutex_rachunek);
				rachunek_grupy += do_zaplaty;
				pthread_mutex_unlock(&mutex_rachunek);

				adres->tasma[moj_id_tasmy].rodzaj = 0;
				adres->tasma[moj_id_tasmy].rezerwacja_dla = 0;
				zjedzone = 1;

				if (wez_cokolwiek) {
					printf("[%s %d] Nie bylo tego co chcialem (%d) , wzialem inne zamowienie standardowe %d\n", czy_vip ? "VIP" : "GRUPA", pid_grupy, typ_zamowiony, typ_zjedzony);
				}
				else {
					if (czy_specjal) {
						printf("[%s %d] Otrzymano zamowienie SPECJALNE %d\n", czy_vip ? "VIP" : "GRUPA", pid_grupy, typ_zjedzony);
					}
					else {
						printf("[%s %d] Otrzymano zamowienie STANDARDOWE %d\n", czy_vip ? "VIP" : "GRUPA", pid_grupy, typ_zjedzony);
					}
				}
			}
			sem_v(sem_id, SEM_BLOKADA);

			if (zjedzone) {
				sem_v(sem_id, SEM_WOLNE);
			}
			else {
				usleep(50000);
			}

		}
		usleep(1000000 + (rand() % 500000));

	}
	free(dane_osoby);
	return NULL;
}

int main() {
	srand(time(NULL) ^ getpid());
	pid_grupy = getpid();

	key_t klucz_shm = ftok(".", ID_PROJEKT);
	key_t klucz_msg = ftok(".", ID_KOLEJKA);
	if (klucz_shm == -1 || klucz_msg == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	int shm_id = custom_error(shmget(klucz_shm, SHM_SIZE, 0600), "Blad shmget | klient");

	sem_id = custom_error(semget(klucz_shm, 0, 0600), "Blad semget | klient");

	int msg_id = custom_error(msgget(klucz_msg, 0600), "Blad msgget | klient");

	adres = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres == (void*)-1) {
		perror("Blad przylaczania do pamieci | klient");
		exit(1);
	}

	KomunikatZaplaty msg;

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

		//Generowanie grupy klientow
		int liczba_osob = 1 + (rand() % 4);
		int dorosli = 1 + (rand() % liczba_osob);
		int dzieci = liczba_osob - dorosli;

	if (czy_vip) {
		printf("[VIP %d] Przychodzi\n", getpid());
	}
	else {
		printf("GRUPA o PID = %d przychodzi\n", getpid());
	}
	fflush(stdout);
	sleep(rand() % 2);

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
			//Zwykly klient


			int czy_lada = 0;

			if (dzieci == 0 && liczba_osob <= 2) {
				if (rand() % 2 == 0) czy_lada = 1;	//50% szans na lade dla grup do 2 osob bez dzieci
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

		int moj_id_tablicy = -1;
		numer_stolika_globalny = 0;

		sem_p(sem_id, SEM_BLOKADA);
		if (semafor_docelowy != SEM_LADA) {
			int start = 0, koniec = 0;

			if (semafor_docelowy == SEM_STOL_1) {
				start = 0;
				koniec = ILOSC_1_OS - 1;
			}
			else if (semafor_docelowy == SEM_STOL_2) {
				start = ILOSC_1_OS;
				koniec = start + ILOSC_2_OS - 1;
			}
			else if (semafor_docelowy == SEM_STOL_3) {
				start = ILOSC_1_OS + ILOSC_2_OS;
				koniec = start + ILOSC_3_OS - 1;
			}
			else {
				start = ILOSC_1_OS + ILOSC_2_OS + ILOSC_3_OS;
				koniec = start + ILOSC_4_OS - 1;
			}

			for (int i = start;i <= koniec;i++) {
				if (adres->stoly[i] == 0) {
					adres->stoly[i] = 1;
					moj_id_tablicy = i;
					numer_stolika_globalny = i + 1;
					break;
				}
			}
			//Zabezpieczenie przed naruszeniem pamieci
			if (moj_id_tablicy == -1)numer_stolika_globalny = 999;
		}
		if (semafor_docelowy != SEM_LADA) {
			printf("[%s %d] Siada przy STOLIKU NR %d\n", czy_vip ? "VIP" : "GRUPA", getpid(), moj_id_tablicy);
		}
		else {
			printf("[%s %d] Siada przy LADZIE\n", czy_vip ? "VIP" : "GRUPA", getpid());
		}
		
		sem_v(sem_id, SEM_BLOKADA);

		//Tworzenie osob w grupie
		pthread_t watki[liczba_osob];

		for (int i = 0;i < liczba_osob;i++) {
			DaneOsoby* x = malloc(sizeof(DaneOsoby));
			x->nr_osoby = i + 1;

			if (i < dorosli) {
				x->czy_dziecko = 0;
			}
			else {
				x->czy_dziecko = 1;
			}

			pthread_create(&watki[i], NULL, zachowanie_osoby, (void*)x);
		}
		
		for (int i = 0;i < liczba_osob;i++) {
			pthread_join(watki[i], NULL);
		}

		//Platnosc
		if (rachunek_grupy > 0 && adres->czy_ewakuacja == 0) {
			msg.mtype = 1;
			msg.pid_klienta = getpid();
			msg.kwota = rachunek_grupy;

			if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
				perror("Blad wyslania zaplaty");
			}
			else {
				printf("[%s %d] Wyslano do kasy %d zl, Wychodzimy\n",czy_vip?"VIP":"GRUPA",getpid(), rachunek_grupy);
			}
		}

		//Zwolnienie id stolika
		if (moj_id_tablicy != -1) {
			sem_p(sem_id, SEM_BLOKADA);
			adres->stoly[moj_id_tablicy] = 0;
			sem_v(sem_id, SEM_BLOKADA);
		}


		sem_zmiana(sem_id, semafor_docelowy, ilosc_do_zajecia);	//Zwolnienie miejsca

		sem_p(sem_id, SEM_BLOKADA);
		adres->liczba_klientow--;
		sem_v(sem_id, SEM_BLOKADA);


	return 0;
}