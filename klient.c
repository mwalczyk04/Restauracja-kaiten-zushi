#include "common.h"
#include <pthread.h>

Restauracja* adres;
int sem_id, msg_id;
int pid_grupy;
int czy_vip = 0;
int rachunek_grupy = 0;
int numer_stolika_globalny = 0;


int sem_private_id; 

#define SEM_PRIV_START 0  // Wątki czekają na start tury
#define SEM_PRIV_DONE  1  // Główny wątek czeka na zakończenie pracy wątków

pthread_mutex_t mutex_rachunek = PTHREAD_MUTEX_INITIALIZER;

// Flagi stanu
volatile int koniec_posilku = 0;
int liczba_najedzonych = 0;
pthread_mutex_t mutex_stan = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int nr_osoby;
    int ile_zje;
    int czy_dziecko; // 1 = Dziecko, 0 = Dorosły
} DaneOsoby;

void* zachowanie_osoby(void* arg) {
    DaneOsoby* dane = (DaneOsoby*)arg;
    
    int moj_specjal = 0;
    int zjedzone = 0;
    int zgloszono_koniec = 0;
    
    char rola[20];
    if (dane->czy_dziecko) sprintf(rola, "Dziecko");
    else sprintf(rola, "Dorosly");

    while (1) {
        
        sem_p(sem_private_id, SEM_PRIV_START);
        
        
        if (koniec_posilku) break;
        
        
        sem_p(sem_id, SEM_BLOKADA);
        
        //Logika jedzenia
        if (zjedzone < dane->ile_zje) {
            
            //Zamawianie specjalne (30% szans)
            if (moj_specjal == 0 && numer_stolika_globalny > 0) {
                if (rand() % 100 < 30) { 
                    for(int i=0; i<MAX_ZAMOWIEN; i++) {
                        if(adres->tablet[i].pid_klienta == 0) {
                            adres->tablet[i].pid_klienta = pid_grupy;
                            adres->tablet[i].typ_dania = 4 + (rand() % 3);
                            moj_specjal = adres->tablet[i].typ_dania; 
                            
                            char* kolor = czy_vip ? K_YELLOW : K_BLUE;
                            printf("%s[GRUPA %d] %s ZAMAWIA Specjalne typ %d\n" K_RESET, 
                                   kolor, pid_grupy, rola, moj_specjal);
                            break;
                        }
                    }
                }
            }

            //Pobieranie z taśmy
            int moj_idx = numer_stolika_globalny % MAX_TASMA;
            Talerz t = adres->tasma[moj_idx];
            int jem = 0;
            
            if (t.rodzaj != 0) {
                if (moj_specjal > 0) {
                    if (t.rezerwacja_dla == pid_grupy && t.rodzaj == moj_specjal) {
                        jem = 1;
                        char* kolor = czy_vip ? K_YELLOW : K_BLUE;
                        printf("%s[GRUPA %d] %s OTRZYMAL Specjalne typ %d\n" K_RESET, 
                               kolor, pid_grupy, rola, t.rodzaj);
                        moj_specjal = 0;
                    }
                } else {
                    if (t.rezerwacja_dla == 0 && t.rodzaj <= 3) {
                        jem = 1;
                        char* kolor = czy_vip ? K_YELLOW : K_BLUE;
                        printf("%s[GRUPA %d] %s Zjadl Standardowe typ %d\n" K_RESET, 
                               kolor, pid_grupy, rola, t.rodzaj);
                    }
                }
            }

            if (jem) {
                int cena = pobierz_cene(t.rodzaj);
                pthread_mutex_lock(&mutex_rachunek);
                rachunek_grupy += cena;
                pthread_mutex_unlock(&mutex_rachunek);
                
                adres->stat_sprzedane[t.rodzaj]++;
                adres->tasma[moj_idx].rodzaj = 0;
                adres->tasma[moj_idx].rezerwacja_dla = 0;
                zjedzone++;
            }
        }
        
        sem_v(sem_id, SEM_BLOKADA);
        
        
        
        if (zjedzone >= dane->ile_zje && !zgloszono_koniec) {
            pthread_mutex_lock(&mutex_stan);
            liczba_najedzonych++;
            zgloszono_koniec = 1;
            pthread_mutex_unlock(&mutex_stan);
        }
        
        
        sem_v(sem_private_id, SEM_PRIV_DONE);
    }
    
    free(dane);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 3) return 0;
    
    srand(time(NULL) ^ getpid());
    pid_grupy = getpid();
    
    int ile_osob = atoi(argv[1]);
    czy_vip = atoi(argv[2]);

    key_t k = ftok(".", ID_PROJEKT);
    int shmid = shmget(k, sizeof(Restauracja), 0600);
    sem_id = semget(k, ILOSC_SEM, 0600);
    msg_id = msgget(ftok(".", ID_KOLEJKA), 0600);
    
    if (shmid == -1 || sem_id == -1 || msg_id == -1) return 0;
    adres = (Restauracja*)shmat(shmid, NULL, 0);
    if (adres == (void*)-1) return 0;

    
    sem_private_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
    if (sem_private_id == -1) { perror("Sem private"); return 1; }
    
    // Inicjalizacja na 0
    semctl(sem_private_id, SEM_PRIV_START, SETVAL, 0);
    semctl(sem_private_id, SEM_PRIV_DONE, SETVAL, 0);

    // Skład grupy
    int dorosli = 1 + (rand() % ile_osob);
    int dzieci = ile_osob - dorosli;

    printf(K_CYAN "[GRUPA %d] Sklad (%d os): %d Doroslych, %d Dzieci\n" K_RESET, 
           pid_grupy, ile_osob, dorosli, dzieci);

    //Wejście do restauracji (Czekanie na stolik)
    sem_p(sem_id, SEM_BLOKADA);
    if (adres->czy_otwarte == 0) {
        sem_v(sem_id, SEM_BLOKADA);
        semctl(sem_private_id, 0, IPC_RMID); // Sprzątamy
        shmdt(adres);
        return 0;
    }
    sem_v(sem_id, SEM_BLOKADA);

    int sem_cel = SEM_STOL_4;
    if (czy_vip) {
        sem_p(sem_id, SEM_BLOKADA);
        if (semctl(sem_id, SEM_STOL_4, GETVAL) > 0) sem_cel = SEM_STOL_4;
        else if (semctl(sem_id, SEM_STOL_3, GETVAL) > 0) sem_cel = SEM_STOL_3;
        else sem_cel = SEM_STOL_2;
        sem_v(sem_id, SEM_BLOKADA);
        printf(K_YELLOW "[VIP %d] Czeka na stolik (Priorytet)\n" K_RESET, pid_grupy);
    } else {
        if (ile_osob <= 2 && rand() % 2 == 0) {
            sem_cel = SEM_LADA;
            numer_stolika_globalny = 0; 
            printf(K_BLUE "[GRUPA %d] Wybiera LADE\n" K_RESET, pid_grupy);
        } else {
            if (ile_osob == 1) sem_cel = SEM_STOL_1;
            else if (ile_osob == 2) sem_cel = SEM_STOL_2;
            else if (ile_osob == 3) sem_cel = SEM_STOL_3;
            else sem_cel = SEM_STOL_4;
            printf(K_BLUE "[GRUPA %d] Wybiera STOLIK\n" K_RESET, pid_grupy);
        }
    }

    // Blokada procesu na wejściu
    sem_op(sem_id, sem_cel, -1);

    if (adres->czy_ewakuacja) {
        sem_op(sem_id, sem_cel, 1);
        semctl(sem_private_id, 0, IPC_RMID);
        shmdt(adres);
        return 0;
    }

    // Zajęcie miejsca w pamięci
    sem_p(sem_id, SEM_BLOKADA);
    int start = 0, koniec = 0;
    if (sem_cel == SEM_LADA) { start = 0; koniec = ILOSC_MIEJSC_LADA; }
    else if (sem_cel == SEM_STOL_1) { start = ILOSC_MIEJSC_LADA; koniec = start + ILOSC_1_OS; }
    else if (sem_cel == SEM_STOL_2) { start = ILOSC_MIEJSC_LADA + ILOSC_1_OS; koniec = start + ILOSC_2_OS; }
    else if (sem_cel == SEM_STOL_3) { start = ILOSC_MIEJSC_LADA + ILOSC_1_OS + ILOSC_2_OS; koniec = start + ILOSC_3_OS; }
    else { start = ILOSC_MIEJSC_LADA + ILOSC_1_OS + ILOSC_2_OS + ILOSC_3_OS; koniec = start + ILOSC_4_OS; }
    
    for(int i=start; i<koniec; i++) {
        if (adres->stoly[i] == 0) {
            adres->stoly[i] = pid_grupy;
            numer_stolika_globalny = i;
            break;
        }
    }
    if (numer_stolika_globalny == 0 && sem_cel != SEM_LADA) numer_stolika_globalny = start;
    
    adres->liczba_aktywnych_grup++;
    sem_v(sem_id, SEM_BLOKADA);

    char* kolor = czy_vip ? K_YELLOW : K_BLUE;
    printf("%s[%s %d] Usiadl przy stoliku nr %d\n" K_RESET, kolor, czy_vip ? "VIP" : "GRUPA", pid_grupy, numer_stolika_globalny);

    //URUCHOMIENIE WĄTKÓW
    pthread_t watki[ile_osob]; 

    for (int i = 0; i < ile_osob; i++) {
        DaneOsoby* x = malloc(sizeof(DaneOsoby));
        x->nr_osoby = i + 1;
        x->ile_zje = 3 + (rand() % 8);

        if (i < dorosli) x->czy_dziecko = 0;
        else x->czy_dziecko = 1;

        pthread_create(&watki[i], NULL, zachowanie_osoby, (void*)x);
    }

    
    while (!adres->czy_ewakuacja) {
        // Czekamy na turę od Obsługi (Globalny)
        sem_op(sem_id, SEM_BAR_START, -1);
        
        if (adres->czy_ewakuacja) {
            koniec_posilku = 1;
            // Odblokuj wątki żeby wyszły
            sem_op_val(sem_private_id, SEM_PRIV_START, ile_osob);
            sem_op(sem_id, SEM_BAR_STOP, 1);
            break;
        }
        
        //  Start Tury Wątków (Prywatny)
        // 
        sem_op_val(sem_private_id, SEM_PRIV_START, ile_osob);
        
        // Czekamy na koniec Tury Wątków (Prywatny)
        // 
        sem_op_val(sem_private_id, SEM_PRIV_DONE, -ile_osob);
        
        //Koniec Tury Globalnej
        sem_op(sem_id, SEM_BAR_STOP, 1);
        
        
        pthread_mutex_lock(&mutex_stan);
        if (liczba_najedzonych >= ile_osob) {
            koniec_posilku = 1;
            
            pthread_mutex_unlock(&mutex_stan);
            break; 
        }
        pthread_mutex_unlock(&mutex_stan);
    }
    
    
    if (!adres->czy_ewakuacja) {
        sem_op_val(sem_private_id, SEM_PRIV_START, ile_osob);
    }

    // Czekanie na zakończenie wątków
    for (int i = 0; i < ile_osob; i++) {
        pthread_join(watki[i], NULL);
    }

    //  Wyjście 
    if (rachunek_grupy > 0 && !adres->czy_ewakuacja) {
        KomunikatZaplaty m = {1, pid_grupy, rachunek_grupy};
        msgsnd(msg_id, &m, sizeof(m) - sizeof(long), 0);
        printf("%s[%s %d] Placi %d zl i wychodzi\n" K_RESET, 
               kolor, czy_vip ? "VIP" : "GRUPA", pid_grupy, rachunek_grupy);
    }

    sem_p(sem_id, SEM_BLOKADA);
    adres->liczba_aktywnych_grup--;
    if(numer_stolika_globalny != 0 && adres->stoly[numer_stolika_globalny] == pid_grupy) {
        adres->stoly[numer_stolika_globalny] = 0;
    }
    sem_v(sem_id, SEM_BLOKADA);

    sem_op(sem_id, sem_cel, 1);
    
    // Usunięcie prywatnych semaforów
    semctl(sem_private_id, 0, IPC_RMID);
    
    shmdt(adres);
    return 0;
}