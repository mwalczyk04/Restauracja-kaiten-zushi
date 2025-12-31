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

// Mozliwosci stolikow
#define ILOSC_1_OS 4
#define ILOSC_2_OS 4
#define ILOSC_3_OS 4
#define ILOSC_4_OS 4

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