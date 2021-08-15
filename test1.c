#include <stdio.h>

int q;
int t;
int c = 3;
double d;

int add(int a, int b)
{
	static int time;

	time++;
	printf("Numbers are added together\n");

	return a + b;
}

int main(void)
{
	int a, b;

	a = 3;
	b = c + 5;
	;

	int ret = add(a, b);
	printf("Result: %d\n", ret);
	printf("q: %d t: %d c: %d\n", q, t, c);

	return 0;
}