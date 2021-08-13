# Use gcc as a compiler
CC=gcc
# CFLAGS will be the options we'll pass to the compiler
CFLAGS = -Wall -Wl,-Ttext-segment=0x30000000
CFLAGS2 = -Wall -Wextra -g 

all : loader demandloader2 hybridloader test

loader : loader.o
	gcc $(CFLAGS) -o loader $<

loader.o : loader.c
	gcc $(CFLAGS2) -c $<

demandloader2 : demandloader2.o
	gcc $(CFLAGS) -o demandloader2 $<

demandloader2.o : demandloader2.c
	gcc $(CFLAGS2) -c $<

hybridloader : hybridloader.o
	gcc $(CFLAGS) -o hybridloader $<

hybridloader.o : hybridloader.c
	gcc $(CFLAGS2) -c $<

test: 
	gcc test1.c -o test1 -static
	gcc test2.c -o test2 -static
	gcc test3.c -o test3 -static
	gcc test4.c -o test4 -static

clean:
	rm -f loader.o
	rm -f loader
	rm -f demandloader2.o
	rm -f demandloader2
	rm -f hybridloader.o 
	rm -f hybridloader
	rm -f test1
	rm -f test2
	rm -f test3
	rm -f test4




