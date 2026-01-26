#include "common.h"

int semid = -1, msgid = -1;
Restauracja* adres = NULL;
int moj_stolik_idx = -1;
int moje_table_id = -1;
int rachunek = 0;
int zjedzone_dania = 0;
int cel_do_zjedzenia = 0;
int pid_grupy;
int ilosc_osob;
int czy_vip;

pthread_mutex_t mutex_grupy = PTHREAD_MUTEX_INITIALIZER;
int koniec_posilku = 0;

typedef struct { int id; int wiek; } DaneKlienta;

void sprobuj_obudzic_kolejnego() {
    for (int t = 0; t < 5; t++) {
        if (adres->waiting_vip[t] > 0) {
            adres->waiting_vip[t]--;
            sem_op(semid, SEM_Q_VIP_LADA + t, 1);
            return;
        }
        else if (adres->waiting_std[t] > 0) {
            adres->waiting_std[t]--;
            sem_op(semid, SEM_Q_STD_LADA + t, 1);
            return;
        }
    }
}

int znajdz_miejsce(int start, int end) {
    int current_tid = -1;

    // 1. VIP (tylko puste)
    if (czy_vip) {
        for (int i = start; i < end; i++) {
            int tid = adres->stol_id[i];
            if (tid == current_tid) continue; current_tid = tid;

            if (adres->stol_zajetosc[tid] == 0) {
                for (int k = i; k < end; k++) {
                    if (adres->stol_id[k] == tid) {
                        moj_stolik_idx = k;
                        break;
                    }
                }
                moje_table_id = tid;
                adres->stol_zajetosc[tid] = ilosc_osob;
                adres->stol_typ_grupy[tid] = ilosc_osob;
                adres->stol_is_vip[tid] = 1;
                return 1; 
            }
        }
        return 0;
    }

    // 2. STANDARD - Dosiadka
    current_tid = -1;
    for (int i = start; i < end; i++) {
        int tid = adres->stol_id[i];
        if (tid == current_tid) continue; current_tid = tid;

        int zaj = adres->stol_zajetosc[tid];

        if (zaj > 0 && adres->stol_is_vip[tid] == 0 &&
            (zaj + ilosc_osob <= adres->stol_pojemnosc[tid]) &&
            (adres->stol_typ_grupy[tid] == ilosc_osob)) { 

            
            for (int k = i; k < end; k++) {
                if (adres->stol_id[k] == tid) {
                    moj_stolik_idx = k + zaj;
                    break;
                }
            }

            moje_table_id = tid;
            adres->stol_zajetosc[tid] += ilosc_osob;
            return 2;
        }
    }

    // 3. STANDARD - Puste
    current_tid = -1;
    for (int i = start; i < end; i++) {
        int tid = adres->stol_id[i];
        if (tid == current_tid) continue; current_tid = tid;

        if (adres->stol_zajetosc[tid] == 0) {
            for (int k = i; k < end; k++) {
                if (adres->stol_id[k] == tid) {
                    moj_stolik_idx = k;
                    break;
                }
            }
            moje_table_id = tid;
            adres->stol_zajetosc[tid] = ilosc_osob;
            adres->stol_typ_grupy[tid] = ilosc_osob;
            adres->stol_is_vip[tid] = 0;
            return 1; 
        }
    }

    return 0;
}

void* zachowanie_klienta(void* arg) {
    DaneKlienta* d = (DaneKlienta*)arg;
    free(d);

    while (1) {
        pthread_mutex_lock(&mutex_grupy);
        if (koniec_posilku || adres->czy_ewakuacja) {
            pthread_mutex_unlock(&mutex_grupy); break;
        }
        pthread_mutex_unlock(&mutex_grupy);

        sem_op(semid, SEM_ACCESS, -1);
        if (moj_stolik_idx >= 0) {
            
            int belt_idx = moj_stolik_idx % MAX_TASMA;
            Talerz* t = &adres->tasma[belt_idx];

            if (t->typ != 0) {
                int czy_biore = 0;
                if (czy_vip) czy_biore = 1;
                else if (t->typ >= 4 && t->pid_rezerwacji == pid_grupy) czy_biore = 1;
                else if (t->typ <= 3 && t->pid_rezerwacji == 0) czy_biore = 1;

                if (czy_biore) {
                    pthread_mutex_lock(&mutex_grupy);
                    if (zjedzone_dania < cel_do_zjedzenia) {
                        rachunek += t->cena;
                        zjedzone_dania++;
                        adres->stat_sprzedane[t->typ]++;
                        t->typ = 0; t->cena = 0; t->pid_rezerwacji = 0;
                        if (zjedzone_dania >= cel_do_zjedzenia) koniec_posilku = 1;
                    }
                    pthread_mutex_unlock(&mutex_grupy);
                }
            }
        }
        sem_op(semid, SEM_ACCESS, 1);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    srand(time(NULL) ^ getpid());
    setbuf(stdout, NULL);
    pid_grupy = getpid();

    ilosc_osob = 1 + (rand() % 4);
    int dorosli = 1 + (rand() % ilosc_osob);
    int dzieci = ilosc_osob - dorosli;

    czy_vip = (rand() % 100 < 5);
    cel_do_zjedzenia = ilosc_osob * (3 + (rand() % 8));

    if (dzieci > 0 && (dorosli * 3 < dzieci)) return 0;

    int wiek_osob[ilosc_osob];
    int w_idx = 0;
    for (int i = 0; i < dorosli; i++) wiek_osob[w_idx++] = 11 + (rand() % 80);
    for (int i = 0; i < dzieci; i++) wiek_osob[w_idx++] = 1 + (rand() % 10);

    key_t klucz = ftok(".", ID_PROJEKT);
    int shmid = shmget(klucz, sizeof(Restauracja), 0600);
    semid = semget(klucz, LICZBA_SEMAFOROW, 0600);
    msgid = msgget(klucz, 0600);
    if (shmid == -1) return 1;
    adres = (Restauracja*)shmat(shmid, NULL, 0);

    
    int typ_miejsca = T_X4, start = 0, end = 0;
    int off_lada = 0, off_x1 = ILE_LADA;
    int off_x2 = off_x1 + ILE_X1, off_x3 = off_x2 + (ILE_X2 * 2);
    int off_x4 = off_x3 + (ILE_X3 * 3), max_idx = off_x4 + (ILE_X4 * 4);

    if (ilosc_osob == 1) {
        if (!czy_vip && (rand() % 100 < 50)) {
            typ_miejsca = T_X2; start = off_x2; end = off_x3;
        }
        else {
            if (!czy_vip && (rand() % 2)) { typ_miejsca = T_LADA; start = off_lada; end = off_x1; }
            else { typ_miejsca = T_X1; start = off_x1; end = off_x2; }
        }
    }
    else if (ilosc_osob == 2) {
        if (rand() % 100 < 50) {
            typ_miejsca = T_X4; start = off_x4; end = max_idx;
        }
        else {
            typ_miejsca = T_X2; start = off_x2; end = off_x3;
        }
    }
    else if (ilosc_osob == 3) { typ_miejsca = T_X3; start = off_x3; end = off_x4; }
    else { typ_miejsca = T_X4; start = off_x4; end = max_idx; }

    int sem_kolejka = czy_vip ? (SEM_Q_VIP_LADA + typ_miejsca) : (SEM_Q_STD_LADA + typ_miejsca);
    int status_miejsca = 0; // 1=Pusty, 2=Dosiadka

    while (1) {
        sem_op(semid, SEM_ACCESS, -1);
        status_miejsca = znajdz_miejsce(start, end);

        if (status_miejsca > 0) {
            sprobuj_obudzic_kolejnego();
            sem_op(semid, SEM_ACCESS, 1);
            break;
        }

        if (czy_vip) adres->waiting_vip[typ_miejsca]++;
        else adres->waiting_std[typ_miejsca]++;
        sem_op(semid, SEM_ACCESS, 1);

        sem_op(semid, sem_kolejka, -1);
        if (adres->czy_ewakuacja) return 0;
    }

    char* kolor = czy_vip ? K_YELLOW : K_CYAN;
    if (status_miejsca == 2) {
        printf("%s[%s %d] DOSIADA SIE do ID %d (Cel: %d) | Wiek: ",
            kolor, czy_vip ? "VIP" : "GRUPA", pid_grupy, moje_table_id, cel_do_zjedzenia);
    }
    else {
        printf("%s[%s %d] SIADA (Pusty) ID %d (Cel: %d) | Wiek: ",
            kolor, czy_vip ? "VIP" : "GRUPA", pid_grupy, moje_table_id, cel_do_zjedzenia);
    }
    for (int i = 0; i < ilosc_osob; i++) printf("%d ", wiek_osob[i]);
    printf("\n" K_RESET);

    if ((rand() % 100) < 50) {
        struct msqid_ds buf;
        msgctl(msgid, IPC_STAT, &buf);
        if (buf.msg_qnum < MAX_ZAMOWIEN_W_KOLEJCE) {
            MsgZamowienie zam = { TYP_ZAMOWIENIE, pid_grupy, 4 + (rand() % 3) };
            msgsnd(msgid, &zam, sizeof(zam) - sizeof(long), 0);
        }
    }

    pthread_t watki[4];
    for (int i = 0; i < ilosc_osob; i++) {
        DaneKlienta* d = malloc(sizeof(DaneKlienta));
        d->id = i + 1;
        d->wiek = wiek_osob[i];
        pthread_create(&watki[i], NULL, zachowanie_klienta, d);
    }

    for (int i = 0; i < ilosc_osob; i++) pthread_join(watki[i], NULL);

    sem_op(semid, SEM_ACCESS, -1);
    if (rachunek > 0) {
        sem_op(semid, SEM_ACCESS, 1);
        MsgPlatnosc req = { KANAL_PLATNOSCI, pid_grupy, rachunek, 0 };
        msgsnd(msgid, &req, sizeof(req) - sizeof(long), 0);
        MsgPlatnosc ack;
        msgrcv(msgid, &ack, sizeof(ack) - sizeof(long), pid_grupy, 0);
        printf(K_GREEN "[%s %d] Zaplacono %d zl.\n" K_RESET, czy_vip ? "VIP" : "GRUPA", pid_grupy, rachunek);
        sem_op(semid, SEM_ACCESS, -1);
    }

    if (moje_table_id != -1) {
        adres->stol_zajetosc[moje_table_id] -= ilosc_osob;
        if (adres->stol_zajetosc[moje_table_id] == 0) {
            adres->stol_typ_grupy[moje_table_id] = 0;
            adres->stol_is_vip[moje_table_id] = 0;
        }
    }

    sprobuj_obudzic_kolejnego();
    sem_op(semid, SEM_ACCESS, 1);
    shmdt(adres);
    return 0;
}