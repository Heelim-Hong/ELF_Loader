// #include <stdio.h>

// void main(int argc, char *argv[], char *envp[])
// {
// 	printf("Hello World %d %s\n", argc, argv[0]);
// }


// #include <stdio.h>
// #include <stdlib.h>

// int global_bss;
// int global_data = 10;

// void print() {
// 	printf("----------------------------------------------\n");
// 	printf("call print()\n");
// 	printf("----------------------------------------------\n");
// }

// int main(int argc, char *argv[]) {
// 	int local = 0;

// 	global_bss = 11;

// 	printf("----------------------------------------------\n");
// 	printf("argc        == %d\n", argc);
// 	for(local = 0; local < argc; local++) printf("argv[%d] : %s\n", local, argv[local]);
// 	printf("----------------------------------------------\n");
// 	printf("global_bss  == %d\n", global_bss);
// 	printf("global_data == %d\n", global_data);
// 	printf("local       == %d\n", local);
// 	printf("----------------------------------------------\n");

// 	print();

// 	exit(0);
// }


// #include <stdio.h>

// int main(int argc, char *argv[])
// {
// 	int x, y, z;
	
// 	x = 1;
// 	y = 2;
// 	z = 3;

// 	printf("This is a test programm\n");
// 	printf("x: %d\n", x);
// 	printf("y: %d\n", y);
// 	printf("z: %d\n", z);
// 	printf("argc: %d\n", argc);

// 	for (int i = 0; i < argc; i++) 
// 		printf("%s \n", argv[i]);

// 	return 0;
// }

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#define TIMES 10
#define SIZE 10000

struct stuff
{
	int data[SIZE];
};

int main(int argc, char* argv[])
{
	struct timeval start_time;
	struct timeval end_time;
	gettimeofday(&start_time, NULL);

	for(unsigned n = 0; n < TIMES; ++n)
	{
		static struct stuff s;
		for(unsigned i = 0; i < SIZE; ++i)
		{
			s.data[i] = i;
			printf("%u\n", i);
		}
	}
	gettimeofday(&end_time, NULL);
	double seconds = difftime(end_time.tv_sec, start_time.tv_sec);
	printf("%f seconds\n", seconds);

	// while(true);
}
