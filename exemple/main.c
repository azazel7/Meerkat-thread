#include <stdio.h>
#include <stdlib.h>
#include "meerkat.h"

#define SIZE 5

static void* func(int *nbb)
{
	int nb = *nbb;
	int i = 1;
	printf("func%d: started\n", nb);
	while(i < 7)
	{
		printf("while %d\n", nb);
		thread_yield();
		/*sleep(1);*/
		++i;
	}
	printf("func%d: returning (%p)\n", nb, nbb);
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
	 /*nb = (int*)malloc(sizeof(int));*/
	/**nb = i++;*/
	 /*func(nb);*/
	int* value;
	for(i = 0; i < SIZE; ++i)
	{
		printf("Joining %d(%d)\n", thre[i], i);
		thread_join(thre[i], (void**)&value);
		printf("Result for %d : %d\n", i, *value);
		free(value);
	}
	fprintf(stdout, "Main ending\n");
	thread_exit(NULL);
	return 0;
	/*
	 * exit(EXIT_SUCCESS);
	 */

	/*
	 * Install timer_handler as the signal handler for SIGVTALRM. 
	 */
	/*
	 * signal(SIGVTALRM, timer_handler);
	 */

	/*
	 * Configure the timer to expire after 250 msec... 
	 */
	/*
	 * timeslice.it_value.tv_sec = 2;
	 */
	/*
	 * timeslice.it_value.tv_usec = 0;
	 */
	/*
	 * timeslice.it_interval = timeslice.it_value;
	 */
	/*
	 * Start a virtual timer. It counts down whenever this process is
	 * *    executing. 
	 */
	/*
	 * setitimer (ITIMER_VIRTUAL, &timeslice, NULL);
	 */

	/*
	 * Do busy work. 
	 */
	/*
	 * while (1);
	 */
}
