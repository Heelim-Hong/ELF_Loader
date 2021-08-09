OPTION = -Wall -Wl,-Ttext-segment=0x30000000
OPTION2 = -Wall -Wextra -g 

all : loader demandloader demandloader2 test1 test2 test3

loader : loader.o
	gcc $(OPTION) -o loader $<

loader.o : loader.c
	gcc $(OPTION2) -c $<

demandloader : demandloader.o
	gcc $(OPTION) -o demandloader $<

demandloader.o : demandloader.c
	gcc $(OPTION2) -c $<

demandloader2 : demandloader2.o
	gcc $(OPTION) -o demandloader2 $<

demandloader2.o : demandloader2.c
	gcc $(OPTION2) -c $<

test1: test1.c 
	gcc test1.c -o test1 -static
	gcc test2.c -o test2 -static
	gcc test3.c -o test3 -static

clean:
	rm -f loader.o
	rm -f loader
	rm -f demandloader.o
	rm -f demandloader
	rm -f demandloader2.o
	rm -f demandloader2
	rm -f test1
	rm -f test2
	rm -f test3


