#include <stdlib.h>
#include <stdio.h>
#include "meerkat.h"
#define SIZE_ARRAY 20
typedef struct qs
{
	int *array;
	int first, last;
} Qs;
static void quicksort(Qs * qs)
{
	if(qs->first >= qs->last)
		return;
	/*thread_yield();*/
	int pivot = qs->first; 
	int i = qs->first, j = qs->last;
	int pivo_value = qs->array[pivot];
	printf("Between [%d,%d]\n", qs->first, qs->last);
	while(i < j)
	{
		while(qs->array[i] <= pivo_value && (i < qs->last))
			++i;
		while(qs->array[j] > pivo_value)
			--j;
		if(i < j)
		{
			printf("[%d,%d] Swap %d et %d\n", qs->first, qs->last, i, j);
			int tmp = qs->array[i];
			qs->array[i] = qs->array[j];
			qs->array[j] = tmp;
			++i;
			--j;
		}
	}
	if(qs->array[j] <= pivo_value)
	{
		int tmp = qs->array[pivot];
		qs->array[pivot] = qs->array[j];
		qs->array[j] = tmp;
		printf("[%d,%d] Swap %d et %d\n", qs->first, qs->last, pivot, j);
	}
	for(i = 0; i < 10; ++i)
		printf("%d ", qs->array[i]);
	printf("\n");
	Qs qs1, qs2;
	qs1.array = qs2.array = qs->array;
	qs1.first = qs->first;
	qs1.last = j - 1;
	qs2.first = j + 1;
	qs2.last = qs->last;
	thread_t t1, t2;
	/*thread_create(&t1, (void *(*)(void *))quicksort, &qs1);*/
	/*thread_create(&t2, (void *(*)(void *))quicksort, &qs2);*/
	/*thread_join(t1, NULL);*/
	/*thread_join(t2, NULL);*/
	quicksort(&qs1);
	quicksort(&qs2);
}

int main(int argc, char** argv)
{
	int size = 30;
	if(argc != 1)
		size = atoi(argv[1]);
	else
		printf("%s <taille tableau>\n", argv[0]);
	int array[size];
	int i;
	for(i = 0; i < size; ++i)
		array[i] = rand()%100;
	for(i = 0; i < size; ++i)
		printf("%d ", array[i]);
	printf("\n");
	Qs qs;
	qs.array = array;
	qs.first = 0;
	qs.last = size - 1;
	quicksort(&qs);
	for(i = 0; i < size; ++i)
		printf("%d ", array[i]);
	printf("\n");
	for(i = 1; i < size; ++i)
		if(array[i - 1] > array[i])
			return -1;
	return 0;
}
