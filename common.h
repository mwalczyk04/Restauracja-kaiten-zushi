#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

// --- KONFIGURACJA ---
#define ID_PROJEKT 'M' 
#define PLIK_RAPORTU "raport.txt"
#define LIMIT_KLIENTOW 10000
#define CZAS_OTWARCIA 30
#define DELAY_BAZA 50000000 
#define MAX_ZAMOWIEN_W_KOLEJCE 20

// Parametry wielkoœci
#define MAX_TASMA 30    // Fizyczna d³ugoœæ taœmy (P)

// Iloœci stolików
#define ILE_LADA 10
#define ILE_X1 5
#define ILE_X2 5
#define ILE_X3 5
#define ILE_X4 5

// Ca³kowita liczba miejsc siedz¹cych (N)
#define MAX_MIEJSC (ILE_LADA + (ILE_X1*1) + (ILE_X2*2) + (ILE_X3*3) + (ILE_X4*4))

// --- SEMAFORY ---
#define SEM_ACCESS      0 

#define SEM_Q_VIP_LADA  1
#define SEM_Q_VIP_X1    2
#define SEM_Q_VIP_X2    3
#define SEM_Q_VIP_X3    4
#define SEM_Q_VIP_X4    5

#define SEM_Q_STD_LADA  6
#define SEM_Q_STD_X1    7
#define SEM_Q_STD_X2    8
#define SEM_Q_STD_X3    9
#define SEM_Q_STD_X4    10

#define LICZBA_SEMAFOROW 11

enum TypMiejsca { T_LADA = 0, T_X1 = 1, T_X2 = 2, T_X3 = 3, T_X4 = 4 };

#define CENA_STD_1 10
#define CENA_STD_2 15
#define CENA_STD_3 20
#define CENA_SPC_1 40
#define CENA_SPC_2 50
#define CENA_SPC_3 60

typedef struct {
    int typ;
    int pid_rezerwacji;
    int cena;
} Talerz;

typedef struct {
    Talerz tasma[MAX_TASMA]; // 30 slotów na jedzenie

    
    int stol_id[MAX_MIEJSC];
    int stol_zajetosc[MAX_MIEJSC];
    int stol_typ_grupy[MAX_MIEJSC];
    int stol_pojemnosc[MAX_MIEJSC];
    int stol_is_vip[MAX_MIEJSC];

    int waiting_vip[5];
    int waiting_std[5];

    int czy_otwarte;
    int czy_wejscie_zamkniete;
    int czy_ewakuacja;
    long utarg_kasa;
    int stat_wyprodukowane[7];
    int stat_sprzedane[7];
    int kucharz_speed;
} Restauracja;

typedef struct {
    long mtype;
    int pid_grupy;
    int typ_dania;
} MsgZamowienie;

typedef struct {
    long mtype;
    int pid_grupy;
    int kwota;
    int is_ack;
} MsgPlatnosc;

#define TYP_ZAMOWIENIE 1
#define KANAL_PLATNOSCI 2

static inline void sem_op(int semid, int sem_num, int op) {
    struct sembuf buf = { sem_num, op, 0 };
    while (1) {
        if (semop(semid, &buf, 1) == 0) break;
        if (errno == EINTR) continue;
        perror("Blad sem_op");
        exit(1);
        //break;
    }
}

static inline void raportuj(const char* format, ...) {
    va_list args, args_file;
    va_start(args, format);
    va_copy(args_file, args);
    vprintf(format, args);
    va_end(args);
    FILE* fp = fopen(PLIK_RAPORTU, "a");
    if (fp) { vfprintf(fp, format, args_file); fclose(fp); }
    else { perror("Blad zapisu raportu"); }
    va_end(args_file);
}
#define printf raportuj

#define K_RESET   "\033[0m"
#define K_RED     "\033[31m"
#define K_GREEN   "\033[32m"
#define K_YELLOW  "\033[33m"
#define K_BLUE    "\033[34m"
#define K_MAGENTA "\033[35m"
#define K_CYAN    "\033[36m"

#endif
