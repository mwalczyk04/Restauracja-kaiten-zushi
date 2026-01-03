#include "common.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define MAX_LICZBA_KLIENTOW 40
#define SHM_SIZE sizeof(Restauracja)

int shm_id = -1;
int sem_id = -1;
Restauracja * adres_restauracji = NULL;

void sprzatanie() {
	
	//printf("\nOtrzymano sygnal %d\n Zaczynam sprzatac\n", sig);
	printf("[Manager] Zamykanie restauracji\n");
	
	kill(0, SIGTERM);	//Sygnal KILL dla wszystkich procesow

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

	if (sem_id != -1) {
		if (semctl(sem_id, 0, IPC_RMID) == -1) {
			perror("Blad usuwania semaforow");
		}
		else {
			printf("Usunieto semafory\n");
		}

	}
	printf("[Manager] Zamykanie zakonczone, zasoby zwolnione\n");

	exit(0);

}

void uruchom_proces(const char* sciezka, const char* nazwa) {
	pid_t pid = fork();

	if (pid == 0) {
		execl(sciezka, nazwa, NULL);

		perror("Blad execl");	//Wykona sie tylko jesli execl nie zadziala
		exit(1);
	}
}

void ustaw_stoliki(Restauracja* r, int ilosc, int pojemnosc, int* index) {
	for (int i = 0; i < ilosc;i++) {
		r->stoliki[*index].id = *index;
		r->stoliki[*index].pojemnosc = pojemnosc;
		r->stoliki[*index].kto_zajmuje = 0;
		r->stoliki[*index].ile_osob_siedzi = 0;
		(*index)++;
	}
}

int main() {
	signal(SIGTERM, SIG_IGN);	//Blokada zeby kill nie zabil Managera
	signal(SIGINT, sprzatanie);// ctrl+c == sprzatanie

	//Ustawienie klucza
	key_t klucz = ftok(".", ID_PROJEKT);
	if (klucz == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	//Ustawienie pamieci
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

	sem_id = semget(klucz, 4, IPC_CREAT | 0666);
	if (sem_id == -1) {
		perror("Blad tworzenia semaforow");
		sprzatanie();
		exit(1);
	}

	//Ustawienie wartosci poczatkowych semaforow
	semctl(sem_id, SEM_BLOKADA, SETVAL, 1);
	semctl(sem_id, SEM_WOLNE, SETVAL, MAX_TASMA);
	semctl(sem_id, SEM_ZAJETE, SETVAL, 0);
	semctl(sem_id, SEM_MIEJSCA, SETVAL, CALKOWITA_ILOSC_MIEJSC);

	//Inicjalizacja pamieci
	adres_restauracji->czy_otwarte = 1;
	adres_restauracji->utarg = 0;

	//Zerowanie tasmy
	for (int i = 0;i < MAX_TASMA;i++) {
		adres_restauracji->tasma[i].rodzaj = 0;
	}

	//Ustawienie stolikow wedlug wytycznych
	int idx = 0;
	ustaw_stoliki(adres_restauracji, ILOSC_1_OS, 1, &idx);
	ustaw_stoliki(adres_restauracji, ILOSC_2_OS, 2, &idx);
	ustaw_stoliki(adres_restauracji, ILOSC_3_OS, 3, &idx);
	ustaw_stoliki(adres_restauracji, ILOSC_4_OS, 4, &idx);

	//Uruchamianie procesow
	uruchom_proces("./kucharz", "kucharz");
	printf("[Manager] Kucharz aktywowany\n");

	printf("[Manager] Aktywowanie klientow\n");
	for (int i = 0;i < MAX_LICZBA_KLIENTOW;i++) {
		uruchom_proces("./klient", "klient");

		usleep(50000);	//Opoznienie 50ms
	}
	

	printf("Restauracja otwarta ID pamieci = %d, ilosc miejsc = %d\n", shm_id, CALKOWITA_ILOSC_MIEJSC);
	
	while (1) {
		sleep(1);
	}

	return 0;
}
