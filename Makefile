all: main

main: main.c common.h
    gcc -Wall -pthread main.c -o main
    gcc kucharz.c -o kucharz
    gcc klient.c -o klient -pthread
    gcc obsluga.c -o obsluga
clean:
    rm -f main
    rm -f kucharz
    rm -f klient
    rm -f obsluga
