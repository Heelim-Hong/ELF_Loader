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
			// printf("%u\n", i);
			printf("%d\n", s.data[i]);
		}
	}
	gettimeofday(&end_time, NULL);
	double seconds = difftime(end_time.tv_sec, start_time.tv_sec);
	printf("%f seconds\n", seconds);

	// while(true);
}
