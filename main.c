#include "common.h"
#include <signal.h>
#include <sys/wait.h>

int shm_id = -1, sem_id = -1, msg_id = -1;
Restauracja * adres_restauracji = NULL;
int pid_kucharza = -1;
int pid_kierownika = -1;
int pid_obslugi = -1;

void sprzatanie() {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    
    printf(K_RED "\n[Manager] Zamykanie restauracji...\n" K_RESET);
    
    // Zakończenie procesów roboczych
    if (pid_obslugi > 0) kill(pid_obslugi, SIGTERM);
    if (pid_kucharza > 0) kill(pid_kucharza, SIGTERM);
    if (pid_kierownika > 0) kill(pid_kierownika, SIGTERM);
    
    // Zabicie pozostałych dzieci
    kill(0, SIGTERM);
    
    // Czekanie na zakończenie
    while(wait(NULL) > 0);
    
    // Zwolnienie zasobów IPC
    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
        printf("[Manager] Pamiec dzielona usunieta\n");
    }
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
        printf("[Manager] Semafory usuniete\n");
    }
    if (msg_id != -1) {
        msgctl(msg_id, IPC_RMID, NULL);
        printf("[Manager] Kolejka komunikatow usunieta\n");
    }
    
    printf(K_RED "[Manager] Zasoby zwolnione.\n" K_RESET);
    exit(0);
}

void handler_ewakuacji(int sig) {
    if (adres_restauracji) {
        adres_restauracji->czy_ewakuacja = 1;
        printf(K_RED "\n\n!!! EWAKUACJA - SYGNAŁ 3 !!!\n\n" K_RESET);
        
        // Obudzenie wszystkich czekających
        if (sem_id != -1) {
            sem_op_val(sem_id, SEM_BAR_START, 1000);
        }
    }
    sprzatanie();
}

int main() {
    setbuf(stdout, NULL);
    srand(time(NULL));
    
    
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, handler_ewakuacji); // Ctrl+C = sygnał 3 = ewakuacja
    
    // Czyszczenie raportu
    FILE* fp = fopen(PLIK_RAPORT, "w");
    if(fp) { 
        fprintf(fp, "========== RAPORT RESTAURACJI ==========\n"); 
        fclose(fp); 
    }

    // Inicjalizacja IPC
    key_t k = ftok(".", ID_PROJEKT);
    shm_id = shmget(k, sizeof(Restauracja), IPC_CREAT | 0600);
    sem_id = semget(k, ILOSC_SEM, IPC_CREAT | 0600);
    msg_id = msgget(ftok(".", ID_KOLEJKA), IPC_CREAT | 0600);
    
    if (shm_id == -1 || sem_id == -1 || msg_id == -1) { 
        perror("Blad IPC (uzyj: ipcrm -a)"); 
        exit(1); 
    }
    
    adres_restauracji = (Restauracja*)shmat(shm_id, NULL, 0);

    // Inicjalizacja pamięci
    adres_restauracji->czy_otwarte = 1;
    adres_restauracji->czy_ewakuacja = 0;
    adres_restauracji->liczba_aktywnych_grup = 0;
    adres_restauracji->utarg = 0;
    adres_restauracji->straty = 0;
    adres_restauracji->kucharz_speed = 0;
    
    for(int i=0; i<MAX_TASMA; i++) { 
        adres_restauracji->tasma[i].rodzaj = 0; 
        adres_restauracji->tasma[i].rezerwacja_dla = 0; 
    }
    
    for(int i=0; i<MAX_LICZBA_STOLIKOW; i++) {
        adres_restauracji->stoly[i] = 0;
    }
    
    for(int i=0; i<7; i++) { 
        adres_restauracji->stat_sprzedane[i] = 0; 
        adres_restauracji->stat_wyprodukowane[i] = 0; 
    }
    
    for(int i=0; i<MAX_ZAMOWIEN; i++) {
        adres_restauracji->tablet[i].pid_klienta = 0;
        adres_restauracji->tablet[i].typ_dania = 0;
    }

    // Inicjalizacja semaforów
    semctl(sem_id, SEM_BLOKADA, SETVAL, 1);
    semctl(sem_id, SEM_BAR_START, SETVAL, 0);
    semctl(sem_id, SEM_BAR_STOP, SETVAL, 0);
    semctl(sem_id, SEM_LADA, SETVAL, ILOSC_MIEJSC_LADA);
    semctl(sem_id, SEM_STOL_1, SETVAL, ILOSC_1_OS);
    semctl(sem_id, SEM_STOL_2, SETVAL, ILOSC_2_OS);
    semctl(sem_id, SEM_STOL_3, SETVAL, ILOSC_3_OS);
    semctl(sem_id, SEM_STOL_4, SETVAL, ILOSC_4_OS);

    // Uruchomienie procesów
    if((pid_kucharza = fork()) == 0) { 
        execl("./kucharz", "kucharz", NULL); 
        perror("Blad execl kucharz");
        exit(1); 
    }
    
    if((pid_obslugi = fork()) == 0) { 
        execl("./obsluga", "obsluga", NULL); 
        perror("Blad execl obsluga");
        exit(1); 
    }
    
    if((pid_kierownika = fork()) == 0) { 
        char buf[20];
        sprintf(buf, "%d", pid_kucharza);
        execl("./kierownik", "kierownik", buf, NULL); 
        perror("Blad execl kierownik");
        exit(1); 
    }
    
    //usleep(100000);

    printf(K_RED "========================================\n" K_RESET);
    printf(K_RED "RESTAURACJA OTWARTA\n" K_RESET);
    printf(K_RED "Czas: %ds | Max procesow: %d\n" K_RESET, CZAS_OTWARCIA, MAX_W_LOKALU);
    printf(K_RED "Miejsca: Lada=%d, 1os=%d, 2os=%d, 3os=%d, 4os=%d\n" K_RESET, 
           ILOSC_MIEJSC_LADA, ILOSC_1_OS, ILOSC_2_OS, ILOSC_3_OS, ILOSC_4_OS);
    printf(K_RED "PID: Kucharz=%d, Obsluga=%d, Kierownik=%d\n" K_RESET,
           pid_kucharza, pid_obslugi, pid_kierownika);
    printf(K_RED "========================================\n" K_RESET);

    int wygenerowani = 0;
    time_t start = time(NULL);
    time_t koniec = start + CZAS_OTWARCIA;

    // Generowanie klientów
    int DEBUG_counter = 0;
    while (time(NULL) < koniec && !adres_restauracji->czy_ewakuacja) {
        // Czyszczenie zombie (teraz działa bo nie ma SIG_IGN)
        int zombie_count = 0;
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            zombie_count++;
        }
        
        if (zombie_count > 0 && DEBUG_counter % 10 == 0) {
            sem_p(sem_id, SEM_BLOKADA);
            int aktualni = adres_restauracji->liczba_aktywnych_grup;
            sem_v(sem_id, SEM_BLOKADA);
            printf(K_RED "[Manager] DEBUG: Posprzatano %d zombie, aktywnych grup: %d\n" K_RESET,
                   zombie_count, aktualni);
        }

        if (wygenerowani < MAX_W_LOKALU && (rand() % 100 < 70)) { // 70% szans na klienta
            // Generuj klienta
            if(fork() == 0) {
                char s_osob[10], s_vip[10];
                int ile_osob = 1 + (rand() % 4); // 1-4 osoby
                int vip = (rand() % 100 < 2); // 2% VIP
                
                sprintf(s_osob, "%d", ile_osob); 
                sprintf(s_vip, "%d", vip); 
                
                execl("./klient", "klient", s_osob, s_vip, NULL);
                perror("Blad execl klient");
                exit(1);
            }
            wygenerowani++;
            
            DEBUG_counter++;
            if (DEBUG_counter % 10 == 0) {
                sem_p(sem_id, SEM_BLOKADA);
                int aktualni = adres_restauracji->liczba_aktywnych_grup;
                sem_v(sem_id, SEM_BLOKADA);
                printf(K_RED "[Manager] DEBUG: Wpuszczono %d grup, w lokalu: %d\n" K_RESET, 
                       wygenerowani, aktualni);
            }
        }
        
        
        BUSY_WAIT(1000000);
    }

    printf(K_RED "\n[Manager] KONIEC CZASU. Drzwi zamkniete.\n" K_RESET);
    printf(K_RED "[Manager] Wpuszczono: %d grup\n" K_RESET, wygenerowani);
    
    adres_restauracji->czy_otwarte = 0;

    
    printf(K_RED "[Manager] Czekam na obsluze wszystkich klientow...\n" K_RESET);
    
    int ostatnia_wartosc = -1;
    while (1) {
        sem_p(sem_id, SEM_BLOKADA);
        int aktualni = adres_restauracji->liczba_aktywnych_grup;
        sem_v(sem_id, SEM_BLOKADA);
        
        if (aktualni != ostatnia_wartosc) {
            printf(K_RED "[Manager] Pozostalo grup: %d\n" K_RESET, aktualni);
            ostatnia_wartosc = aktualni;
        }
        
        if (aktualni <= 0) break;
        
        
        BUSY_WAIT(1000000);
    }

    printf(K_RED "[Manager] Wszyscy klienci obsluzeni i wyszli.\n" K_RESET);
    
    
    BUSY_WAIT(2000000);
    
    // Wyczyść wszystkie pozostałe zombie
    while (waitpid(-1, NULL, WNOHANG) > 0);

    
    adres_restauracji->czy_ewakuacja = 1;
    sem_op_val(sem_id, SEM_BAR_START, 100);

    // Raport końcowy
    printf("\n");
    printf(K_RED "========== RAPORT KONCOWY ==========\n" K_RESET);
    printf(K_RED "Wpuszczono grup: %d\n" K_RESET, wygenerowani);
    printf(K_RED "Utarg: %d zl\n" K_RESET, adres_restauracji->utarg);
    printf(K_RED "Straty: %d zl\n" K_RESET, adres_restauracji->straty);
    
    printf(K_RED "\nSprzedane dania:\n" K_RESET);
    int suma_sprzedaz = 0;
    for(int i=1; i<=6; i++) {
        if (adres_restauracji->stat_sprzedane[i] > 0) {
            int wartosc = adres_restauracji->stat_sprzedane[i] * pobierz_cene(i);
            printf(K_RED " - Typ %d: %d szt (%d zl)\n" K_RESET, 
                   i, adres_restauracji->stat_sprzedane[i], wartosc);
            suma_sprzedaz += wartosc;
        }
    }
    printf(K_RED "Suma sprzedazy: %d zl\n" K_RESET, suma_sprzedaz);
    
    printf(K_RED "\nWyprodukowane dania:\n" K_RESET);
    int suma_produkcja = 0;
    for(int i=1; i<=6; i++) {
        if (adres_restauracji->stat_wyprodukowane[i] > 0) {
            int wartosc = adres_restauracji->stat_wyprodukowane[i] * pobierz_cene(i);
            printf(K_RED " - Typ %d: %d szt (%d zl)\n" K_RESET, 
                   i, adres_restauracji->stat_wyprodukowane[i], wartosc);
            suma_produkcja += wartosc;
        }
    }
    printf(K_RED "Suma produkcji: %d zl\n" K_RESET, suma_produkcja);
    printf(K_RED "====================================\n" K_RESET);

    sprzatanie();
    return 0;
}
