#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>


#define SIZE 1000000

int main(int argc, char* argv[])
{
	struct timeval start_time;
	struct timeval end_time;
	gettimeofday(&start_time, NULL);

	int* values = (int*) malloc(sizeof(int) * SIZE);
	for(unsigned i = 0; i < SIZE; ++i)
	{
		values[i] = i;
		printf("%d\n", values[i]);
	}

	gettimeofday(&end_time, NULL);
	double seconds = difftime(end_time.tv_sec, start_time.tv_sec);
	printf("%f seconds\n", seconds);
	// while(true);
}
