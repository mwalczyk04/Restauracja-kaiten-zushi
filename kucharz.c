#include "common.h"

Restauracja* adres_global = NULL;

int main() {
    setbuf(stdout, NULL);
    srand(time(NULL) ^ getpid());
    key_t klucz = ftok(".", ID_PROJEKT);
    int shmid = shmget(klucz, sizeof(Restauracja), 0600);
    int semid = semget(klucz, LICZBA_SEMAFOROW, 0600);
    int msgid = msgget(klucz, 0600);
    adres_global = (Restauracja*)shmat(shmid, NULL, 0);

    printf(K_MAGENTA "[KUCHARZ] Gotowy.\n" K_RESET);

    while (adres_global->czy_otwarte && !adres_global->czy_ewakuacja) {
        sem_op(semid, SEM_ACCESS, -1);

        if (adres_global->tasma[0].typ == 0) {
            int typ = 0, pid = 0;

           
            struct msqid_ds buf;
            msgctl(msgid, IPC_STAT, &buf);

            if (buf.msg_qnum > 0) {
                // Próba odbioru
                MsgZamowienie zam;
                if (msgrcv(msgid, &zam, sizeof(MsgZamowienie) - sizeof(long), TYP_ZAMOWIENIE, IPC_NOWAIT) != -1) {
                    typ = zam.typ_dania; pid = zam.pid_grupy;
                    printf(K_MAGENTA "[KUCHARZ] Specjal %d dla %d\n " K_RESET, typ,pid);
                }
                else {
                    typ = (rand() % 3) + 1;
                }
            }
            else {
                typ = (rand() % 3) + 1;
            }

            adres_global->tasma[0].typ = typ;
            adres_global->tasma[0].pid_rezerwacji = pid;
            if (typ == 1) adres_global->tasma[0].cena = CENA_STD_1;
            else if (typ == 2) adres_global->tasma[0].cena = CENA_STD_2;
            else if (typ == 3) adres_global->tasma[0].cena = CENA_STD_3;
            else if (typ == 4) adres_global->tasma[0].cena = CENA_SPC_1;
            else if (typ == 5) adres_global->tasma[0].cena = CENA_SPC_2;
            else if (typ == 6) adres_global->tasma[0].cena = CENA_SPC_3;

            adres_global->stat_wyprodukowane[typ]++;
        }
        sem_op(semid, SEM_ACCESS, 1);

        long delay = DELAY_BAZA;
        if (adres_global->kucharz_speed == 1) delay /= 2;
        if (adres_global->kucharz_speed == 2) delay *= 2;
        // for(volatile long k=0; k<delay; k++);
    }
    shmdt(adres_global);
    return 0;
}
