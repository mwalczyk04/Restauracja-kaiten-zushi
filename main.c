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

	sem_id = semget(klucz, 8, IPC_CREAT | 0666);
	if (sem_id == -1) {
		perror("Blad tworzenia semaforow");
		sprzatanie();
		exit(1);
	}

	//Ustawienie wartosci poczatkowych semaforow
	semctl(sem_id, SEM_BLOKADA, SETVAL, 1);
	semctl(sem_id, SEM_WOLNE, SETVAL, MAX_TASMA);
	semctl(sem_id, SEM_ZAJETE, SETVAL, 0);

	semctl(sem_id, SEM_LADA, SETVAL, ILOSC_MIEJSC_LADA);
	semctl(sem_id, SEM_STOL_1, SETVAL, ILOSC_1_OS);
	semctl(sem_id, SEM_STOL_2, SETVAL, ILOSC_2_OS * 2);
	semctl(sem_id, SEM_STOL_3, SETVAL, ILOSC_3_OS * 3);
	semctl(sem_id, SEM_STOL_4, SETVAL, ILOSC_4_OS * 4);

	//Inicjalizacja pamieci
	adres_restauracji->czy_otwarte = 1;
	adres_restauracji->utarg = 0;

	//Zerowanie tasmy
	for (int i = 0;i < MAX_TASMA;i++) {
		adres_restauracji->tasma[i].rodzaj = 0;
		adres_restauracji->tasma[i].rezerwacja_dla = 0;
	}
	//Zerowanie tabletu
	for (int i = 0; i < MAX_ZAMOWIEN; i++) {
		adres_restauracji->tablet[i].pid_klienta = 0;
		adres_restauracji->tablet[i].typ_dania = 0;
	}

	//Uruchamianie procesow
	uruchom_proces("./kucharz", "kucharz");
	printf("[Manager] Kucharz aktywowany\n");

	printf("[Manager] Aktywowanie klientow\n");
	for (int i = 0;i < MAX_LICZBA_KLIENTOW;i++) {
		uruchom_proces("./klient", "klient");

		usleep(50000);	//Opoznienie 50ms
	}
	

	printf("Restauracja otwarta ID pamieci = %d, miejsce: LADA = %d , 1 OS = %d , 2 OS = %d , 3 OS = %d , 4 OS = %d\n", shm_id, ILOSC_MIEJSC_LADA, ILOSC_1_OS, ILOSC_2_OS, ILOSC_3_OS, ILOSC_4_OS);
	
	while (1) {
		sleep(1);
	}

	return 0;
}
