#ifndef COMMON_H
#define COMMON_H
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
#include <sys/wait.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

// --- KONFIGURACJA ---
#define MAX_TASMA 20
#define ID_PROJEKT 'O'
#define ID_KOLEJKA 'K'
#define MAX_ZAMOWIEN 10
#define REZERWA_DLA_SPECJAL 3

// Limity
#define DOCELOWO_KLIENTOW 10000 
#define MAX_W_LOKALU 500
#define CZAS_OTWARCIA 30

// Semafory
#define SEM_BLOKADA 0
#define SEM_BAR_START 1
#define SEM_BAR_STOP 2
#define SEM_LADA 3
#define SEM_STOL_1 4
#define SEM_STOL_2 5
#define SEM_STOL_3 6
#define SEM_STOL_4 7
#define ILOSC_SEM 8

// Lokal
#define ILOSC_1_OS 4
#define ILOSC_2_OS 4
#define ILOSC_3_OS 4
#define ILOSC_4_OS 4
#define ILOSC_MIEJSC_LADA 10
#define MAX_LICZBA_STOLIKOW (ILOSC_MIEJSC_LADA + ILOSC_1_OS + ILOSC_2_OS + ILOSC_3_OS + ILOSC_4_OS)

// Ceny
#define CENA_DANIA_1 10
#define CENA_DANIA_2 15
#define CENA_DANIA_3 20
#define CENA_DANIA_4 40
#define CENA_DANIA_5 50
#define CENA_DANIA_6 60

#define PLIK_RAPORT "raport.txt"

// Kolory
#define K_RED     "\x1B[31m"
#define K_GREEN   "\x1B[32m"
#define K_YELLOW  "\x1B[33m"
#define K_BLUE    "\x1B[34m"
#define K_MAGENTA "\x1B[35m"
#define K_CYAN    "\x1B[36m"
#define K_RESET   "\033[0m"

#define TRYB_BEZ_SLEEP 

#ifdef TRYB_BEZ_SLEEP
#define sleep(x) ((void)0)
#define usleep(x) ((void)0)
#endif

#define BUSY_WAIT(cycles) do { for(volatile long _i = 0; _i < (cycles); _i++); } while(0)

typedef struct {
    long mtype;
    int pid_klienta;
    int kwota;
} KomunikatZaplaty;

typedef struct {
    int rodzaj; 
    int rezerwacja_dla;
} Talerz;

typedef struct {
    int pid_klienta;
    int typ_dania;
} Zamowienie;

typedef struct {
    Talerz tasma[MAX_TASMA];
    Zamowienie tablet[MAX_ZAMOWIEN];
    
    int czy_otwarte;
    int utarg;
    int czy_ewakuacja;
    int liczba_aktywnych_grup; 
    int stoly[MAX_LICZBA_STOLIKOW];
    
    // Statystyki
    int stat_sprzedane[7];
    int stat_wyprodukowane[7];
    int straty;
    int kucharz_speed; // 0=normal, 1=2x szybki, 2=0.5x wolny
} Restauracja;

static inline int custom_error(int wynik, const char* msg) {
    if (wynik == -1) { perror(msg); exit(EXIT_FAILURE); }
    return wynik;
}

static inline void raportuj(const char* format, ...) {
    va_list args, args2;
    va_start(args, format);
    va_copy(args2, args);
    
    vprintf(format, args);
    
    FILE* fp = fopen(PLIK_RAPORT, "a");
    if (fp != NULL) {
        vfprintf(fp, format, args2);
        fclose(fp);
    }
    va_end(args);
    va_end(args2);
}

#define printf raportuj 

static inline void sem_op(int sem_id, int sem_num, int op) {
    struct sembuf buf = {sem_num, op, 0};
    while (semop(sem_id, &buf, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) break;
    }
}

static inline void sem_op_val(int sem_id, int sem_num, int val) {
    struct sembuf buf = {sem_num, val, 0};
    while (semop(sem_id, &buf, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) break;
    }
}

static inline void sem_p(int sem_id, int sem_num) { sem_op(sem_id, sem_num, -1); }
static inline void sem_v(int sem_id, int sem_num) { sem_op(sem_id, sem_num, 1); }

static inline int pobierz_cene(int typ) {
    switch(typ) {
        case 1: return CENA_DANIA_1;
        case 2: return CENA_DANIA_2;
        case 3: return CENA_DANIA_3;
        case 4: return CENA_DANIA_4;
        case 5: return CENA_DANIA_5;
        case 6: return CENA_DANIA_6;
        default: return 0;
    }
}

#endif
