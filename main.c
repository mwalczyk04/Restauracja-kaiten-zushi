#include "common.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define MAX_LICZBA_KLIENTOW 40
#define SHM_SIZE sizeof(Restauracja)
#define CZAS_OTWARCIA 30	//Czas mierzony w sekundach

int shm_id = -1;
int sem_id = -1;
int msg_id = -1;
Restauracja * adres_restauracji = NULL;


void sprzatanie() {
	
	//printf("\nOtrzymano sygnal %d\n Zaczynam sprzatac\n", sig);
	printf("[Manager] Zamykanie restauracji\n");
	
	kill(0, SIGTERM);	//Sygnal KILL dla wszystkich procesow
	sleep(1);

	

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

	if (msg_id != -1) {
		if (msgctl(msg_id, IPC_RMID, NULL) == -1) {
			perror("Blad usuwania kolejki komunikatow");
		}
		else {
			printf("Usunieto kolejke komunikatow\n");
		}
	}
	printf("[Manager] Zamykanie zakonczone, zasoby zwolnione\n");

	exit(0);

}

void naped_tasmy() {
	printf("[Manager] Tasma ruszyla (PID %d)\n", getpid());

	while (adres_restauracji->czy_otwarte == 1 || adres_restauracji->liczba_klientow > 0) {

		if (adres_restauracji->czy_ewakuacja)break;

		usleep(500000);

		struct sembuf tasma_blokada;
		tasma_blokada.sem_num = SEM_BLOKADA;
		tasma_blokada.sem_op = -1;
		tasma_blokada.sem_flg = 0;
		if (semop(sem_id, &tasma_blokada, 1) == -1) {
			//Ignorowanie bladu usuniecia semafora 
			if (errno == EIDRM || errno == EINVAL) {
				exit(0);
			}
			perror("Blad tasma semafor");
			exit(1);
		}

		Talerz ostatni = adres_restauracji->tasma[MAX_TASMA - 1];

		for (int i = MAX_TASMA - 1;i > 0;i--) {
			adres_restauracji->tasma[i] = adres_restauracji->tasma[i - 1];
		}
		adres_restauracji->tasma[0] = ostatni;

		struct sembuf tasma_odblokowana;
		tasma_odblokowana.sem_num = SEM_BLOKADA;
		tasma_odblokowana.sem_op = 1;
		tasma_odblokowana.sem_flg = 0;
		if (semop(sem_id, &tasma_odblokowana, 1) == -1) {
			//Ignorowanie bladu usuniecia semafora 
			if (errno == EIDRM || errno == EINVAL) {
				exit(0);
			}
			perror("Blad tasma semafor");
			exit(1);
		}
	}
	printf("[Manager] Tasma sie zatrzymala\n");
	exit(0);
}


void uruchom_proces(const char* sciezka, const char* nazwa) {
	pid_t pid = custom_error(fork(), "Blad fork");

	if (pid == 0) {
		custom_error(execl(sciezka, nazwa, NULL), "Blad execl");
	}
}

void obsluga_sygnalu_3(int sig) {
	if (adres_restauracji != NULL) {
		adres_restauracji->czy_ewakuacja = 1;
		printf("\n[Manager] Otrzymano sygnal 3. Ewakuacja\n\n");
	}
	sprzatanie();
}

int main() {
	signal(SIGTERM, SIG_IGN);	//Blokada zeby kill nie zabil Managera
	signal(SIGINT, obsluga_sygnalu_3);// ctrl+c == sygnal 3 ewakuacja

	FILE* fp = fopen("raport.txt", "w");	//Czyszczenie pliku na start
	if (fp) {
		fprintf(fp, "Raport logow restauracji\n");
		fclose(fp);
	}

	//Ustawienie klucza
	key_t klucz_shm = ftok(".", ID_PROJEKT);
	key_t klucz_msg = ftok(".", ID_KOLEJKA);
	if (klucz_shm == -1 || klucz_msg == -1) {
		perror("Blad utworzenia klucza!");
		exit(1);
	}

	//Ustawienie pamieci
	shm_id = shmget(klucz_shm, SHM_SIZE, IPC_CREAT | 0600);
	if (shm_id == -1) {
		perror("Blad podlaczania segmentu pamieci dzielonej!");
		sprzatanie();
		exit(1);
	}

	adres_restauracji = (Restauracja*)shmat(shm_id, NULL, 0);
	if (adres_restauracji == (void*)-1) {
		perror("Blad przylaczenia (shmat)");
		shmctl(shm_id, IPC_RMID, NULL);
		exit(1);
	}

	sem_id = semget(klucz_shm, 8, IPC_CREAT | 0600);
	if (sem_id == -1) {
		perror("Blad tworzenia semaforow");
		sprzatanie();
		exit(1);
	}

	msg_id = msgget(klucz_msg, IPC_CREAT | 0600);
	if (msg_id == -1) {
		perror("Blad msgget");
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
	adres_restauracji->czy_ewakuacja = 0;
	adres_restauracji->czy_otwarte = 1;
	adres_restauracji->utarg = 0;
	adres_restauracji->liczba_klientow = 0;

	for (int i = 0;i < MAX_LICZBA_STOLIKOW;i++) {
		adres_restauracji->stoly[i] = 0;
	}

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
	if (fork() == 0) {
		naped_tasmy();	//Uruchomienei tasmy w osobnym procesie
	}

	//Uruchamianie procesow
	uruchom_proces("./kucharz", "kucharz");
	printf("[Manager] Kucharz aktywowany\n");

	uruchom_proces("./obsluga", "obsluga");
	printf("[Manager] Obsluga aktywna\n");

	printf("Restauracja otwarta ID pamieci = %d, miejsce: LADA = %d , 1 OS = %d , 2 OS = %d , 3 OS = %d , 4 OS = %d\n", shm_id, ILOSC_MIEJSC_LADA, ILOSC_1_OS, ILOSC_2_OS, ILOSC_3_OS, ILOSC_4_OS);
	
	for (int t = 0;t < CZAS_OTWARCIA;t++) {

		if (rand() % 100 < 75) {	//75% ze klient sie pojawi	//Moze potem zmienic
			uruchom_proces("./klient", "klient");
		}
		sleep(1);
		//printf("[Zegar] Do zamkniecia %d\n", CZAS_OTWARCIA - t);
	}
	printf("[Manager] Koniec czasu! Drzwi zamkniete\n");
	adres_restauracji->czy_otwarte = 0;

	while (adres_restauracji->liczba_klientow > 0) {
		printf("[Manager] Pozostalo %d grup w srodku\n", adres_restauracji->liczba_klientow);
		sleep(1);
	}
	sleep(1);
	sprzatanie();

	return 0;
}
