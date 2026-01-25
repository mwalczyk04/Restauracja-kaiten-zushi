#include "common.h"

int main() {
    setbuf(stdout, NULL);
    key_t klucz = ftok(".", ID_PROJEKT);
    int shmid = shmget(klucz, sizeof(Restauracja), 0600);
    int semid = semget(klucz, LICZBA_SEMAFOROW, 0600);
    int msgid = msgget(klucz, 0600);
    Restauracja* adres = (Restauracja*)shmat(shmid, NULL, 0);

    printf(K_GREEN "[OBSLUGA] Ready.\n" K_RESET);

    while (adres->czy_otwarte && !adres->czy_ewakuacja) {
        sem_op(semid, SEM_ACCESS, -1);
        Talerz last = adres->tasma[MAX_TASMA - 1];
        for (int i = MAX_TASMA - 1; i > 0; i--) adres->tasma[i] = adres->tasma[i - 1];
        adres->tasma[0] = last;
        sem_op(semid, SEM_ACCESS, 1);

        //Sprawdzenie czy jest jakaœ p³atnoœæ
        struct msqid_ds buf;
        msgctl(msgid, IPC_STAT, &buf);

        if (buf.msg_qnum > 0) {
            MsgPlatnosc msg;
            // Odbieranie p³atnoœci (typ 2)
            if (msgrcv(msgid, &msg, sizeof(MsgPlatnosc) - sizeof(long), KANAL_PLATNOSCI, IPC_NOWAIT) != -1) {
                sem_op(semid, SEM_ACCESS, -1);
                adres->utarg_kasa += msg.kwota;
                sem_op(semid, SEM_ACCESS, 1);

                MsgPlatnosc reply = { msg.pid_grupy, msg.pid_grupy, 0, 1 };
                msgsnd(msgid, &reply, sizeof(reply) - sizeof(long), 0);
            }
        }

        long delay = DELAY_BAZA;
        if (adres->kucharz_speed == 1) delay /= 4;
        if (adres->kucharz_speed == 2) delay *= 4;
        // for(volatile long k=0; k<delay; k++);
    }

    long w = 0;
    for (int i = 0; i < MAX_TASMA; i++) if (adres->tasma[i].typ) w += adres->tasma[i].cena;
    printf(K_GREEN "\n[OBSLUGA] Koniec. Zmarnowano: %ld zl\n" K_RESET, w);
    shmdt(adres);
    return 0;
}
