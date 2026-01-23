#include "common.h"

#include <signal.h>



Restauracja* adres_global = NULL;

int sem_id_global = -1;



void handler_sigterm(int sig) {

    if (sem_id_global != -1) sem_p(sem_id_global, SEM_BLOKADA);

    

    printf(K_GREEN "\n========== RAPORT OBSLUGI/KASY ==========\n" K_RESET);

    if (adres_global) {

       

        printf(K_GREEN "PRODUKTY SPRZEDANE (KASA):\n" K_RESET);

        int utarg_suma = 0;

        for(int i=1; i<=6; i++) {

            if (adres_global->stat_sprzedane[i] > 0) {

                int cena = pobierz_cene(i);

                int wartosc = adres_global->stat_sprzedane[i] * cena;

                printf(K_GREEN "Danie typ %d: %d szt x %d zl = %d zl\n" K_RESET, 

                       i, adres_global->stat_sprzedane[i], cena, wartosc);

                utarg_suma += wartosc;

            }

        }

        printf(K_GREEN "UTARG (z komunikatow): %d zl\n" K_RESET, adres_global->utarg);

        printf(K_GREEN "UTARG (wyliczony): %d zl\n" K_RESET, utarg_suma);

        

        

        printf(K_GREEN "\nPRODUKTY POZOSTALE NA TASMIE:\n" K_RESET);

        int straty = 0;

        int znaleziono = 0;

        

        for(int i=0; i<MAX_TASMA; i++) {

            if (adres_global->tasma[i].rodzaj != 0) {

                int cena = pobierz_cene(adres_global->tasma[i].rodzaj);

                printf(K_GREEN " - Pozycja %d: typ %d (%d zl)\n" K_RESET, 

                       i, adres_global->tasma[i].rodzaj, cena);

                straty += cena;

                znaleziono = 1;

            }

        }

        

        if (!znaleziono) {

            printf(K_GREEN " (brak - tasma pusta)\n" K_RESET);

        } else {

            printf(K_GREEN "KWOTA ZMARNOWANA: %d zl\n" K_RESET, straty);

        }

        

        adres_global->straty = straty;

    }

    printf(K_GREEN "=========================================\n" K_RESET);

    

    if (sem_id_global != -1) sem_v(sem_id_global, SEM_BLOKADA);

    _exit(0);

}



int main() {

    setbuf(stdout, NULL);

    signal(SIGINT, SIG_IGN);

    signal(SIGTERM, handler_sigterm);

    

    key_t k = ftok(".", ID_PROJEKT);

    int shmid = shmget(k, sizeof(Restauracja), 0600);

    int semid = semget(k, ILOSC_SEM, 0600);

    int msgid = msgget(ftok(".", ID_KOLEJKA), 0600);

    

    Restauracja* adres = (Restauracja*)shmat(shmid, NULL, 0);

    adres_global = adres;

    sem_id_global = semid;

    

    printf(K_GREEN "[Obsluga] Kasjer + tasma gotowa\n" K_RESET);

    

    while (!adres->czy_ewakuacja) {

        // Odbieranie płatności

        KomunikatZaplaty msg;

        while (msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT) != -1) {

            sem_p(semid, SEM_BLOKADA);

            adres->utarg += msg.kwota;

            printf(K_GREEN "[Kasa] Wplyw %d zl od grupy %d\n" K_RESET, msg.kwota, msg.pid_klienta);

            sem_v(semid, SEM_BLOKADA);

        }

        

        BUSY_WAIT(5000000); 

        

        // Przesunięcie taśmy

        sem_p(semid, SEM_BLOKADA);

        

        Talerz ostatni = adres->tasma[MAX_TASMA - 1];

        

        for(int i = MAX_TASMA - 1; i > 0; i--) {

            adres->tasma[i] = adres->tasma[i - 1];

        }

        

        adres->tasma[0] = ostatni;

        

        int n = 1 + adres->liczba_aktywnych_grup;

        sem_v(semid, SEM_BLOKADA);

        

    

        // OBUDZENIE wszystkich czekających (kucharz + klienci)

        sem_op_val(semid, SEM_BAR_START, n);

        

        // CZEKANIE aż wszyscy się obudzą i zakończą turę

        for(int i = 0; i < n; i++) {

            sem_op(semid, SEM_BAR_STOP, -1);

        }

    }

    

    return 0;

}

