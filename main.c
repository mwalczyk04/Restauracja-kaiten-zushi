#include "common.h"

int shmid = -1, semid = -1, msgid = -1;
Restauracja* adres = NULL;
pid_t pid_kucharz = 0, pid_obsluga = 0;

void cleanup() {
    if (pid_kucharz > 0) kill(pid_kucharz, SIGTERM);
    if (pid_obsluga > 0) kill(pid_obsluga, SIGTERM);
    while (wait(NULL) > 0);
    if (adres) shmdt(adres);
    if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
    if (msgid != -1) msgctl(msgid, IPC_RMID, NULL);
    printf(K_RED "\n[MANAGER] Koniec symulacji.\n" K_RESET);
}

void handler_sigint(int sig) {
    if (adres) adres->czy_ewakuacja = 1;
    for (int i = 1; i < LICZBA_SEMAFOROW; i++) sem_op(semid, i, 1000);
    printf(K_RED "\n[MANAGER] EWAKUACJA!\n" K_RESET);
    cleanup();
    exit(0);
}

int main() {
    setbuf(stdout, NULL);
    FILE* fp = fopen(PLIK_RAPORTU, "w");
    if (fp) { fprintf(fp, "=== RAPORT ===\n\n"); fclose(fp); }

    signal(SIGINT, handler_sigint);
    srand(time(NULL));

    key_t klucz = ftok(".", ID_PROJEKT);
    shmid = shmget(klucz, sizeof(Restauracja), IPC_CREAT | 0600);
    semid = semget(klucz, LICZBA_SEMAFOROW, IPC_CREAT | 0600);
    msgid = msgget(klucz, IPC_CREAT | 0600);

    if (shmid == -1 || semid == -1 || msgid == -1) { perror("IPC"); exit(1); }
    adres = (Restauracja*)shmat(shmid, NULL, 0);

    adres->czy_otwarte = 1; adres->czy_wejscie_zamkniete = 0;
    adres->czy_ewakuacja = 0; adres->utarg_kasa = 0;

    // Taœma (jedzenie) - rozmiar MAX_TASMA 
    for (int i = 0; i < MAX_TASMA; i++) {
        adres->tasma[i].typ = 0;
    }
    // Stoliki (ludzie) - rozmiar MAX_MIEJSC 
    for (int i = 0; i < MAX_MIEJSC; i++) {
        adres->stol_zajetosc[i] = 0;
        adres->stol_typ_grupy[i] = 0;
        adres->stol_is_vip[i] = 0;
    }
    for (int i = 0; i < 5; i++) { adres->waiting_vip[i] = 0; adres->waiting_std[i] = 0; }
    for (int i = 0; i < 7; i++) { adres->stat_wyprodukowane[i] = 0; adres->stat_sprzedane[i] = 0; }

    int global_idx = 0, table_id = 0;

    //Inicjalizacja stolikow i lady
    for (int i = 0; i < ILE_LADA; i++) {
        adres->stol_id[global_idx++] = table_id;
        adres->stol_pojemnosc[table_id++] = 1;
    }
    
    for (int i = 0; i < ILE_X1; i++) {
        adres->stol_id[global_idx++] = table_id;
        adres->stol_pojemnosc[table_id++] = 1;
    }
    
    for (int i = 0; i < ILE_X2; i++) {
        for (int k = 0; k < 2; k++) adres->stol_id[global_idx++] = table_id;
        adres->stol_pojemnosc[table_id++] = 2;
    }
    
    for (int i = 0; i < ILE_X3; i++) {
        for (int k = 0; k < 3; k++) adres->stol_id[global_idx++] = table_id;
        adres->stol_pojemnosc[table_id++] = 3;
    }
    
    for (int i = 0; i < ILE_X4; i++) {
        for (int k = 0; k < 4; k++) adres->stol_id[global_idx++] = table_id;
        adres->stol_pojemnosc[table_id++] = 4;
    }

    // Init Semafory
    semctl(semid, SEM_ACCESS, SETVAL, 1);
    for (int i = 1; i < LICZBA_SEMAFOROW; i++) semctl(semid, i, SETVAL, 0);

    printf(K_RED "[MANAGER] Start. Miejsc: %d, Tasma: %d \n" K_RESET, MAX_MIEJSC, MAX_TASMA);

    if ((pid_kucharz = fork()) == 0) { execl("./kucharz", "kucharz", NULL); exit(1); }
    if ((pid_obsluga = fork()) == 0) { execl("./obsluga", "obsluga", NULL); exit(1); }

    time_t start = time(NULL);
    int wygenerowani = 0, aktywni = 0;

    while (time(NULL) - start < CZAS_OTWARCIA && !adres->czy_ewakuacja && wygenerowani < LIMIT_KLIENTOW) {
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) aktywni--;

        if ((rand() % 100) < 40) {
            if (fork() == 0) {
                //char arg_ile[5], arg_vip[2];
                //sprintf(arg_ile, "%d", (rand() % 4) + 1);
                //sprintf(arg_vip, "%d", (rand() % 100 < 5));
                //execl("./klient", "klient", arg_ile, arg_vip, NULL);
                execl("./klient", "klient", NULL);
                exit(1);
            }
            wygenerowani++; aktywni++;
        }
    }

    printf(K_RED "\n[MANAGER] Koniec wpuszczania. Czekam na %d.\n" K_RESET, aktywni);
    adres->czy_wejscie_zamkniete = 1;

    // Próba obudzenia wszystkich (na wszelki wypadek)
    for (int t = 0; t < 5; t++) {
        if (adres->waiting_vip[t] > 0) sem_op(semid, SEM_Q_VIP_LADA + t, 1);
        if (adres->waiting_std[t] > 0) sem_op(semid, SEM_Q_STD_LADA + t, 1);
    }

    while (aktywni > 0) {
        pid_t p = wait(NULL);
        if (p > 0) {
            if (p != pid_kucharz && p != pid_obsluga) {
                aktywni--;
                if (aktywni % 10 == 0) printf(K_YELLOW "." K_RESET);
            }
        }
        else if (errno == ECHILD) break;
    }

    struct msqid_ds buf;
    do {
        msgctl(msgid, IPC_STAT, &buf);
       
    } while (buf.msg_qnum > 0);

    printf(K_RED "\n[MANAGER] Pusto. Raport.\n" K_RESET);
    adres->czy_otwarte = 0;

    printf(K_RED"\n== RAPORT KONCOWY DNIA ===\n"K_RESET);

    //   for(volatile long k=0; k<50000000; k++);

    int ceny[] = { 0,CENA_STD_1, CENA_STD_2, CENA_STD_3, CENA_SPC_1, CENA_SPC_2, CENA_SPC_3 };
    
    printf(K_MAGENTA"\n--- RAPORT KUCHARZ (PRODUKCJA) ---\n");
    long suma_produkcji = 0;
    for (int i = 1; i < 7; i++) {
        int ilosc = adres->stat_wyprodukowane[i];
        int wartosc = ilosc * ceny[i];
        suma_produkcji += wartosc;
        printf("Typ %d: %4d szt. x %2d zl = %5d zl\n", i, ilosc, ceny[i], wartosc);
    }
    printf("LACZNA WARTOSC PRODUKCJI: %ld zl\n"K_RESET, suma_produkcji);

    printf(K_GREEN"\n--- RAPORT OBSLUGA (SPRZEDAZ) ---\n");
    long suma_sprzedazy = 0;
    for (int i = 1; i < 7; i++) {
        int ilosc = adres->stat_sprzedane[i];
        int wartosc = ilosc * ceny[i];
        suma_sprzedazy += wartosc;
        printf("Typ %d: %4d szt. x %2d zl = %5d zl\n", i, ilosc, ceny[i], wartosc);
    }
    printf("LACZNA WARTOSC SPRZEDAZY: %ld zl\n", suma_sprzedazy);
    printf("STAN KASY (UTARG):        %ld zl\n", adres->utarg_kasa);

    printf("\n--- ZAWARTOSC TASMY (STRATY) ---\n");
    long straty = 0;
    int znaleziono = 0;
    for (int i = 0; i < MAX_TASMA; i++) {
        if (adres->tasma[i].typ != 0) {
            printf("Slot %2d: Danie Typ %d (%d zl)\n", i, adres->tasma[i].typ, adres->tasma[i].cena);
            straty += adres->tasma[i].cena;
            znaleziono = 1;
        }
    }

    if (!znaleziono) {
        printf("Tasma pusta, brak strat.\n");
    }
    else {
        printf("LACZNE STRATY: %ld zl\n", straty);
    }
    printf("=============================\n"K_RESET);

    cleanup();
    return 0;
}
