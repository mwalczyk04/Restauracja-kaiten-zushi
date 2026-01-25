#include "common.h"
#include <signal.h>

int pid_kucharza = -1;

void handler(int sig) {
    if (sig == SIGUSR1) {
        if (pid_kucharza > 0) {
            kill(pid_kucharza, sig);
            printf(K_CYAN "[Kierownik] SYGNAL 1 - Przyspiesz kuchnie 2x!\n" K_RESET);
        }
    } else if (sig == SIGUSR2) {
        if (pid_kucharza > 0) {
            kill(pid_kucharza, sig);
            printf(K_CYAN "[Kierownik] SYGNAL 2 - Zmniejsz produkcje do 50%%!\n" K_RESET);
        }
    } else if (sig == SIGTERM) {
        printf(K_CYAN "[Kierownik] Koniec zmiany.\n" K_RESET);
        _exit(0);
    }
}

int main(int argc, char** argv) {
    signal(SIGINT, SIG_IGN);
    signal(SIGUSR1, handler);
    signal(SIGUSR2, handler);
    signal(SIGTERM, handler);
    
    if (argc > 1) {
        pid_kucharza = atoi(argv[1]);
    }
    
    printf(K_CYAN "[Kierownik] Czuwam nad lokalem (PID=%d, Kucharz=%d)\n" K_RESET, getpid(), pid_kucharza);
    printf(K_CYAN "[Kierownik] Sygnaly: SIGUSR1=przyspiesz, SIGUSR2=zwolnij\n" K_RESET);
    
    while(1) pause();
    
    return 0;
}
