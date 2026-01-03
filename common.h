#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define MAX_STOLIKI 20
#define MAX_TASMA 20
#define ID_PROJEKT 'V'

#define SEM_BLOKADA 0	//Blokada tasmy
#define SEM_WOLNE 1
#define SEM_ZAJETE 2
#define SEM_MIEJSCA 3

// Mozliwosci stolikow
#define ILOSC_1_OS 4
#define ILOSC_2_OS 4
#define ILOSC_3_OS 4
#define ILOSC_4_OS 4
#define CALKOWITA_ILOSC_MIEJSC 30

//Cennik dan
#define CENA_DANIA_1 10;
#define CENA_DANIA_2 15;
#define CENA_DANIA_3 20;

typedef struct {
	int rodzaj; // Kolor talerza , moze zmienic potem 
	int cena;
	int rezerwacja_dla;  
}Talerz;

typedef struct {
	int id;
	int pojemnosc;
	int kto_zajmuje;
	int ile_osob_siedzi;
}Stolik;

typedef struct {
	Talerz tasma[MAX_TASMA];
	Stolik stoliki[MAX_STOLIKI];
	int czy_otwarte;
	int utarg;
}Restauracja;

void sem_p(int sem_id, int sem_num) {
	struct sembuf buf;
	buf.sem_num = sem_num;
	buf.sem_op = -1;
	buf.sem_flg = 0;
	if (semop(sem_id, &buf, 1) == -1) {
		perror("Blad sem_p");
		exit(1);
	}
	else {
		printf("Semafor zostal zamkniety\n");
	}
}

void sem_v(int sem_id, int sem_num) {
	struct sembuf buf;
	buf.sem_num = sem_num;
	buf.sem_op = 1;
	buf.sem_flg = 0;
	if (semop(sem_id, &buf, 1) == -1) {
		perror("Blad sem_v");
		exit(1);
	}
	else {
		printf("Semafor zostal otwarty\n");
	}
}