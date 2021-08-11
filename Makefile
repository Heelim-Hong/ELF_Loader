OPTION = -Wall -Wl,-Ttext-segment=0x30000000
OPTION2 = -Wall -Wextra -g 

all : loader demandloader demandloader2 hybridloader test

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

hybridloader : hybridloader.o
	gcc $(OPTION) -o hybridloader $<

hybridloader.o : hybridloader.c
	gcc $(OPTION2) -c $<

test: 
	gcc test1.c -o test1 -static
	gcc test2.c -o test2 -static
	gcc test3.c -o test3 -static
	gcc test4.c -o test4 -static
	gcc test5.c -o test5 -static
	gcc test6.c -o test6 -static

clean:
	rm -f loader.o
	rm -f loader
	rm -f demandloader.o
	rm -f demandloader
	rm -f demandloader2.o
	rm -f demandloader2
	rm -f hybridloader.o 
	rm -f hybridloader
	rm -f test1
	rm -f test2
	rm -f test3
	rm -f test4
	rm -f test5
	rm -f test6




