#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/msg.h>
#include <stdarg.h>

#define MAX_TASMA 20
#define ID_PROJEKT 'V'	//Litera do ftok semaforow
#define ID_KOLEJKA 'K' //Litera do  ftok komunikatow
#define MAX_ZAMOWIEN 10	//Limit zamowien na tablecie

#define SEM_BLOKADA 0	//Blokada tasmy
#define SEM_WOLNE 1
#define SEM_ZAJETE 2

//Semafory do kolejek do stolikow
#define SEM_LADA 3
#define SEM_STOL_1 4
#define SEM_STOL_2 5
#define SEM_STOL_3 6
#define SEM_STOL_4 7
#define MAX_LICZBA_STOLIKOW 50

// Mozliwosci stolikow
#define ILOSC_1_OS 4
#define ILOSC_2_OS 4
#define ILOSC_3_OS 4
#define ILOSC_4_OS 4
#define ILOSC_MIEJSC_LADA 10

//Cennik dan
//Podstawowe
#define CENA_DANIA_1 10
#define CENA_DANIA_2 15
#define CENA_DANIA_3 20
//Specjalne z tabletu
#define CENA_DANIA_4 40
#define CENA_DANIA_5 50
#define CENA_DANIA_6 60

#define PLIK_RAPORT "raport.txt"


typedef struct {
	long mtype;	//Typ komunikatu
	int pid_klienta;
	int kwota;
}KomunikatZaplaty;

typedef struct {
	int rodzaj; // Kolor talerza , moze zmienic potem 
	int cena;
	int rezerwacja_dla;	// 0 = dla wszystkich, wartosc = zarezerwowane
}Talerz;


typedef struct {
	int pid_klienta;
	int typ_dania;
}Zamowienie;

typedef struct {
	Talerz tasma[MAX_TASMA];
	Zamowienie tablet[MAX_ZAMOWIEN];
	int czy_otwarte;
	int utarg;
	int liczba_klientow;
	int czy_ewakuacja;	//0 = Normalny koniec 1 = Ewakuacja
	int licznik_numer_stolika;
}Restauracja;


static void sem_p(int sem_id, int sem_num) {
	struct sembuf buf;
	buf.sem_num = sem_num;
	buf.sem_op = -1;
	buf.sem_flg = 0;
	if (semop(sem_id, &buf, 1) == -1) {
		//Ignorowanie bladu usuniecia semafora 
		if (errno == EIDRM || errno == EINVAL) {
			exit(0);
		}
		perror("Blad sem_p");
		exit(1);
	}
	
}

static void sem_v(int sem_id, int sem_num) {
	struct sembuf buf;
	buf.sem_num = sem_num;
	buf.sem_op = 1;
	buf.sem_flg = 0;
	if (semop(sem_id, &buf, 1) == -1) {
		if (errno == EIDRM || errno == EINVAL) {
			exit(0);
		}
		perror("Blad sem_v");
		exit(1);
	}
	
}

static void sem_zmiana(int sem_id, int sem_num, int delta) {
	struct sembuf buf;
	buf.sem_num = sem_num;
	buf.sem_op = delta;
	buf.sem_flg = 0;
	if (semop(sem_id, &buf, 1) == -1) {
		if (errno == EIDRM || errno == EINVAL) {
			exit(0);
		}
		perror("Blad sem_zmiana");
		exit(1);
	}
}

static void raportuj(const char* format, ...) {
	va_list args;

	//Wypisanie na ekran
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	//Wypisanie do pliku
	FILE* fp = fopen(PLIK_RAPORT, "a");	//Tryb dopisywania
	if (fp != NULL) {
		va_start(args, format);
		vfprintf(fp, format, args);
		va_end(args);
	//Wymuszenie zapisywania dla bezpieczenstwa
	fflush(fp);
	fclose(fp);
	}
}

#define printf raportuj	//Podmienia kazdy printf na funkcje raportuj