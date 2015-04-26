#include <stdio.h>
#include <stdlib.h>
#include "meerkat.h"

#define SIZE 5

static void* func(int *nbb)
{
	int nb = *nbb;
	int i = 1;
	printf("func%d: started (%p)\n", nb, (void*)thread_self());
	while(i < 7)
	{
		printf("while %p\n", (void*)thread_self());
		thread_yield();
		/*sleep(1);*/
		++i;
	}
	printf("func%d: returning (%p)\n", nb, (void*)thread_self());
	*nbb *= 3;
	return (void*)nbb;
}

int main(int argc, char *argv[])
{
	printf("MySelf : %p\n", thread_self());
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
		printf("Joining %p i'm %p\n", (void*)thre[i], (void*)thread_self());
		thread_join(thre[i], (void**)&value);
		printf("Result for %d : %d\n", i, *value);
		free(value);
	}
	fprintf(stdout, "Main ending\n");
	thread_exit(NULL);
}
