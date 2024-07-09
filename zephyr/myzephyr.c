#include <stdio.h>
#include <stdlib.h>

void swap(int *a, int *b)
{
	int c;
	c = *a;
	*a = *b;
	*b = c;
}

int main(int argc, char **argv)
{
	int a,b;
	scanf("%d %d", &a, &b);
	printf("before sawp a = %d, b = %d\n", a, b);
	swap(&a, &b);
	printf("after swap a = %d, b = %d\n", a, b);

	exit(0);
}
