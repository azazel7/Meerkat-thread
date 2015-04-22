#include <stdio.h>
#include <stdlib.h>
#include "meerkat.h"

#define SIZE 5

static void* func(int *nbb)
{
	int nb = *nbb;
	int i = 1;
	printf("func%d: started (%d)\n", nb, (int)thread_self());
	while(i < 7)
	{
		printf("while %d\n", (int)thread_self());
		thread_yield();
		/*sleep(1);*/
		++i;
	}
	printf("func%d: returning (%d)\n", nb, (int)thread_self());
	*nbb *= 3;
	return (void*)nbb;
}

int main(int argc, char *argv[])
{
	int i = 0, *nb;
	thread_t thre[SIZE];
	for(i = 0; i < SIZE; i++)
	{
		nb = (int *)malloc(sizeof(int));
		*nb = i;
		if(thread_create(&thre[i], (void* (*)(void*))func, nb) < 0)
			break;
		thread_yield();
	}
	int* value;
	for(i = 0; i < SIZE; ++i)
	{
		printf("Joining %d i'm %d\n", (int)thre[i], (int)thread_self());
		thread_join(thre[i], (void**)&value);
		printf("Result for %d : %d\n", i, *value);
		free(value);
	}
	fprintf(stdout, "Main ending\n");
	thread_exit(NULL);
}
