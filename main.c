#include "common.h"
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LICZBA_KLIENTOW 100		// Ilosc wygenerowanych razem klientow
#define MAX_AKTYWNYCH_KLIENTOW 200	// Ilosc osob w lokalu
#define SHM_SIZE sizeof(Restauracja)
#define CZAS_OTWARCIA 30	//Czas mierzony w sekundach

int shm_id = -1;
int sem_id = -1;
int msg_id = -1;
Restauracja * adres_restauracji = NULL;


void sprzatanie() {
	
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

	if (sem_id != -1) sem_p(sem_id, SEM_BLOKADA);
	printf(K_RED"[Manager] Zamykanie restauracji\n"K_RESET);
	if (sem_id != -1) sem_v(sem_id, SEM_BLOKADA);

	kill(0, SIGTERM);	//Sygnal KILL dla wszystkich procesow
	while (wait(NULL) > 0);
	

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
		shm_id = -1;	//Zabezpieczenie przed ponownym usunieciem
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
	printf(K_RED"[Manager] Zamykanie zakonczone, zasoby zwolnione\n"K_RESET);

	exit(0);

}


void naped_tasmy() {
	signal(SIGINT, SIG_IGN);
	printf(K_RED"[Manager] Tasma ruszyla (PID %d)\n"K_RESET, getpid());

	struct sembuf wez_blokade = { SEM_BLOKADA, -1, 0 };
	struct sembuf oddaj_blokade = { SEM_BLOKADA, 1, 0 };

	while (adres_restauracji->czy_otwarte == 1 || adres_restauracji->liczba_klientow > 0) {
		if (adres_restauracji->czy_ewakuacja) break;

		if (semctl(sem_id, SEM_ZMIANA, SETVAL, 0) == -1) {
			if (errno != EIDRM && errno != EINVAL) perror("Blad semctl reset");
		}

		//if (semop(sem_id, &wez_blokade, 1) == -1) exit(1);
		while (semop(sem_id, &wez_blokade, 1) == -1) {
			if (errno == EINTR) continue;
			if (errno == EIDRM || errno == EINVAL) exit(0);
			perror("Blad tasma blokuj"); exit(1);
		}


		Talerz ostatni = adres_restauracji->tasma[MAX_TASMA - 1];

		for (int i = MAX_TASMA - 1;i > 0;i--) {
			adres_restauracji->tasma[i] = adres_restauracji->tasma[i - 1];
		}
		adres_restauracji->tasma[0] = ostatni;

		while (semop(sem_id, &oddaj_blokade, 1) == -1) {
			if (errno == EINTR) continue;
			if (errno == EIDRM || errno == EINVAL) exit(0);
			perror("Blad tasma odblokuj"); exit(1);
		}

		int ile_obudzic = adres_restauracji->liczba_klientow + 5;
		if (ile_obudzic > 500) ile_obudzic = 500;
		//if (semop(sem_id, &oddaj_blokade, 1) == -1) exit(1);

		if (semctl(sem_id, SEM_ZMIANA, SETVAL, ile_obudzic) == -1) {
			if (errno != EIDRM && errno != EINVAL) perror("Blad semctl wake");
		}
		for (volatile int i = 0; i < 5000000; i++);
	}

	//semctl(sem_id, SEM_ZMIANA, SETVAL, 1000);
	printf(K_RED"[Manager] Tasma sie zatrzymala\n"K_RESET);
	exit(0);
}


int uruchom_proces(const char* sciezka, const char* nazwa) {
	pid_t pid = fork();

	if (pid == 0) {
		//custom_error(execl(sciezka, nazwa, NULL), "Blad execl");
		execl(sciezka, nazwa, NULL);
		perror("Blad execl");
		exit(1);
	}
	else if (pid == -1) {
		//fprintf(stderr, K_RED"[Manager] OSTRZEZENIE!!! System przciazony (fork == -1)\n"K_RESET);
		return -1;
	}
	return pid;
}

void obsluga_sygnalu_3(int sig) {
	if (adres_restauracji != NULL) {
		adres_restauracji->czy_ewakuacja = 1;
		printf(K_RED"\n[Manager] Otrzymano sygnal 3. Ewakuacja\n\n"K_RESET);
	}
	sprzatanie();
}

int main() {
	signal(SIGCHLD, SIG_IGN);	//Sprzatanie zombie
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

	sem_id = semget(klucz_shm, 14, IPC_CREAT | 0600);
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
	semctl(sem_id, SEM_WYJSCIE, SETVAL, 0);
	semctl(sem_id, SEM_ZMIANA, SETVAL, 0);

	semctl(sem_id, SEM_LADA, SETVAL, ILOSC_MIEJSC_LADA);
	semctl(sem_id, SEM_STOL_1, SETVAL, ILOSC_1_OS);
	semctl(sem_id, SEM_STOL_2, SETVAL, ILOSC_2_OS * 2);
	semctl(sem_id, SEM_STOL_3, SETVAL, ILOSC_3_OS * 3);
	semctl(sem_id, SEM_STOL_4, SETVAL, ILOSC_4_OS * 4);

	semctl(sem_id, SEM_BRAMKA_1, SETVAL, 1);
	semctl(sem_id, SEM_BRAMKA_2, SETVAL, 1);
	semctl(sem_id, SEM_BRAMKA_3, SETVAL, 1);
	semctl(sem_id, SEM_BRAMKA_4, SETVAL, 1);

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

	//PIPE dla kucharza
	int potok_kucharz[2];
	custom_error(pipe(potok_kucharz), "Blad pipe");

	pid_t pid_kucharz = custom_error(fork(), "Blad fork kucharz");

	if (pid_kucharz == 0) {
		close(potok_kucharz[1]);

		custom_error(dup2(potok_kucharz[0], STDIN_FILENO), "Blad dup2");

		close(potok_kucharz[0]);//Zamkniecie orginalu

		execl("./kucharz", "kucharz", NULL);
		perror("Blad execl");
		exit(1);
	}
	else {
		close(potok_kucharz[0]);

		int seed = time(NULL);	//Generowanie losowego ziarna
		write(potok_kucharz[1], &seed, sizeof(int));	//Wysylanie do kucharza

		close(potok_kucharz[1]);

		printf(K_RED"[Manager] Kucharz aktywowany\n"K_RESET);
	}


	//Uruchamianie procesow
	uruchom_proces("./obsluga", "obsluga");
	printf(K_RED"[Manager] Obsluga aktywna\n"K_RESET);

	printf(K_RED"Restauracja otwarta ID pamieci = %d, miejsce: LADA = %d , 1 OS = %d , 2 OS = %d , 3 OS = %d , 4 OS = %d\n"K_RESET, shm_id, ILOSC_MIEJSC_LADA, ILOSC_1_OS, ILOSC_2_OS, ILOSC_3_OS, ILOSC_4_OS);
	
	int wpuszczenie_klienci = 0;
	int aktywni_klienci = 0;

	time_t czas_start = time(NULL);
	time_t czas_koniec = czas_start + CZAS_OTWARCIA;

	while (time(NULL) < czas_koniec) {
		if (adres_restauracji->czy_ewakuacja) break;

		sem_p(sem_id, SEM_BLOKADA);
		aktywni_klienci = adres_restauracji->liczba_klientow;
		sem_v(sem_id, SEM_BLOKADA);

		if (wpuszczenie_klienci < MAX_LICZBA_KLIENTOW && aktywni_klienci<MAX_AKTYWNYCH_KLIENTOW) {
			if (rand() % 100 < 75) {	//75% ze klient sie pojawi	//Moze potem zmienic
				int pid_nowego = uruchom_proces("./klient", "klient");
				if (pid_nowego == -1) {
					printf("Limit fork osi¹gniêty\n");
				}
				else {
					wpuszczenie_klienci++;

				}
			}
			sleep(1);

		}
	}
	printf(K_RED"[Manager] Koniec czasu! Drzwi zamkniete\n"K_RESET);
	sem_p(sem_id, SEM_BLOKADA);
	adres_restauracji->czy_otwarte = 0;
	sem_v(sem_id, SEM_BLOKADA);

	while (1) {
		
		sem_p(sem_id, SEM_BLOKADA);
		int ile_w_srodku = adres_restauracji->liczba_klientow;
		sem_v(sem_id, SEM_BLOKADA);

		if (ile_w_srodku <= 0) {
			break;
		}
		sem_p(sem_id, SEM_WYJSCIE);
	}

	sleep(1);
	sprzatanie();

	return 0;
}
