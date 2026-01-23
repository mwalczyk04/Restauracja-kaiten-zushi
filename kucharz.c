#include "common.h"
#include <signal.h>

Restauracja* adres_global = NULL;
int sem_id_global = -1;

void handler_sygnalow(int sig) {
    if (sig == SIGUSR1) {
        if (adres_global) adres_global->kucharz_speed = 1; // 2x szybciej
        printf(K_MAGENTA "\n[Kucharz] SYGNAŁ 1 - Przyspiesznie produkcji 2x!\n" K_RESET);
    } else if (sig == SIGUSR2) {
        if (adres_global) adres_global->kucharz_speed = 2; // 0.5x wolniej (zmniejsz 50%)
        printf(K_MAGENTA "\n[Kucharz] SYGNAŁ 2 - Zmniejszenie produkcji do 50%%!\n" K_RESET);
    } else if (sig == SIGTERM) {
        if (sem_id_global != -1) sem_p(sem_id_global, SEM_BLOKADA);
        
        printf(K_MAGENTA "\n========== RAPORT KUCHARZA ==========\n" K_RESET);
        printf(K_MAGENTA "PRODUKTY WYTWORZONE/UMIESZCZONE NA TASMIE:\n" K_RESET);
        if (adres_global) {
            int suma = 0;
            for(int i=1; i<=6; i++) {
                if (adres_global->stat_wyprodukowane[i] > 0) {
                    int cena = pobierz_cene(i);
                    int wartosc = adres_global->stat_wyprodukowane[i] * cena;
                    printf(K_MAGENTA "Danie typ %d: %d szt x %d zl = %d zl\n" K_RESET, 
                           i, adres_global->stat_wyprodukowane[i], cena, wartosc);
                    suma += wartosc;
                }
            }
            printf(K_MAGENTA "KWOTA SUMARYCZNA: %d zl\n" K_RESET, suma);
        }
        printf(K_MAGENTA "=====================================\n" K_RESET);
        
        if (sem_id_global != -1) sem_v(sem_id_global, SEM_BLOKADA);
        _exit(0);
    }
}

int main() {
    signal(SIGINT, SIG_IGN);
    signal(SIGUSR1, handler_sygnalow);
    signal(SIGUSR2, handler_sygnalow);
    signal(SIGTERM, handler_sygnalow);
    
    srand(time(NULL) ^ getpid());
    
    key_t k = ftok(".", ID_PROJEKT);
    int shmid = shmget(k, sizeof(Restauracja), 0600);
    int semid = semget(k, ILOSC_SEM, 0600);
    
    adres_global = (Restauracja*)shmat(shmid, NULL, 0);
    sem_id_global = semid;
    
    printf(K_MAGENTA "[Kucharz] Rozpoczynam prace PID = %d\n" K_RESET, getpid());
    
    int cooldown = 0;
    
    while (!adres_global->czy_ewakuacja) {
        sem_op(semid, SEM_BAR_START, -1);
        if (adres_global->czy_ewakuacja) { 
            sem_op(semid, SEM_BAR_STOP, 1); 
            break; 
        }
        
        sem_p(semid, SEM_BLOKADA);
        
        // Sprawdzenie prędkości
        int delay = 0;
        if (adres_global->kucharz_speed == 1) delay = -1; // Szybciej - cooldown -1
        else if (adres_global->kucharz_speed == 2) delay = 2; // Wolniej - cooldown +2
        
        if (cooldown <= 0) {
            int typ = 0, kto = 0;
            int tablet_idx = -1;
            
            //PRIORYTET: Zamówienia specjalne z tabletu
            for(int i=0; i<MAX_ZAMOWIEN; i++) {
                if(adres_global->tablet[i].pid_klienta != 0) {
                    typ = adres_global->tablet[i].typ_dania;
                    kto = adres_global->tablet[i].pid_klienta;
                    tablet_idx = i;
                    break;
                }
            }
            
            // Produkcja standardowa (tylko jeśli nie ma specjalnych)
            if (typ == 0) {
                // Liczenie ile standardowych na taśmie
                int ile_standardowych = 0;
                for(int j=0; j<MAX_TASMA; j++) {
                    if (adres_global->tasma[j].rodzaj > 0 && 
                        adres_global->tasma[j].rodzaj <= 3) {
                        ile_standardowych++;
                    }
                }
                
                // Limit standardowych 
                if (ile_standardowych < (MAX_TASMA - REZERWA_DLA_SPECJAL)) {
                    typ = 1 + (rand() % 3);
                }
            }
            
            
            if (typ > 0 && adres_global->tasma[0].rodzaj == 0) {
                adres_global->tasma[0].rodzaj = typ;
                adres_global->tasma[0].rezerwacja_dla = kto;
                adres_global->stat_wyprodukowane[typ]++;
                
                if (tablet_idx != -1) {
                    adres_global->tablet[tablet_idx].pid_klienta = 0;
                    adres_global->tablet[tablet_idx].typ_dania = 0;
                }
                
                if(kto) 
                    printf(K_MAGENTA "[Kucharz] SPECJALNE typ %d dla grupy %d\n" K_RESET, typ, kto);
                else 
                    printf(K_MAGENTA "[Kucharz] Standardowe typ %d\n" K_RESET, typ);
                
                cooldown = delay;
            }
        } else {
            cooldown--;
        }
        
        sem_v(semid, SEM_BLOKADA);
        sem_op(semid, SEM_BAR_STOP, 1);
    }
    
    return 0;
}
