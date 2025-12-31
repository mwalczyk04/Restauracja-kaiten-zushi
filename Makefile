all: main

main: main.c common.h
        gcc -Wall -pthread main.c -o main

clean:
        rm -f main
