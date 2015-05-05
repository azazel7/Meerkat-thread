#include <ucontext.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <valgrind/valgrind.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include "list.h"
#include "runqueue.h"
#include "global.h"
#include "mutex.h"

#define MAX_SIZE_LOCAL_RUNQUEUE 50 

extern LIST_HEAD(thread_u, runqueue);
extern mutex_t runqueue_mutex;
extern sem_t *semaphore_runqueue;
extern core_information *core;
extern int number_of_core;
unsigned int runqueue_size = 0;

thread_u *get_thread_from_runqueue(int id_core)
{
	if(CURRENT_CORE.runqueue == NULL)
	{
		int i = 0;
		sem_wait(semaphore_runqueue);
		//Lock the runqueue so even if there is many thread in the runqueue, only one will modify the runqueue
		mutex_lock(&runqueue_mutex);
		int to_get = (runqueue_size / number_of_core);	
		
		if(to_get == 0)
			to_get = 1;
		if(to_get > MAX_SIZE_LOCAL_RUNQUEUE)
			to_get = MAX_SIZE_LOCAL_RUNQUEUE;
		for(; i < to_get; ++i)
		{
			thread_u* tmp = runqueue;
			LIST_REMOVE(runqueue, runqueue, runqueue);
			LIST_APPEND(runqueue, CURRENT_CORE.runqueue, tmp);
			if(i != 0) //Already done the sem_wait for the first
				sem_wait(semaphore_runqueue);
			--runqueue_size; //No need of atomic because we have a mutex
		}
		mutex_unlock(&runqueue_mutex);
	}
	thread_u* tmp = CURRENT_CORE.runqueue;
	LIST_REMOVE(runqueue, CURRENT_CORE.runqueue, CURRENT_CORE.runqueue);
	return tmp;
}

thread_u* try_get_thread_from_runqueue(int id_core)
{
	if(CURRENT_CORE.runqueue == NULL)
	{
		int i = 0;
		if(sem_trywait(semaphore_runqueue) != 0)
		{
			return NULL;
		}
		//Lock the runqueue so even if there is many thread in the runqueue, only one will modify the runqueue
		mutex_lock(&runqueue_mutex);
		int to_get = (runqueue_size / number_of_core);	
		if(to_get == 0)
			to_get = 1;
		if(to_get > MAX_SIZE_LOCAL_RUNQUEUE)
			to_get = MAX_SIZE_LOCAL_RUNQUEUE;
		for(; i < to_get; ++i)
		{
			thread_u* tmp = runqueue;
			LIST_REMOVE(runqueue, runqueue, runqueue);
			LIST_APPEND(runqueue, CURRENT_CORE.runqueue, tmp);
			if(i != 0) //Already done the sem_wait for the first
				sem_wait(semaphore_runqueue);
			--runqueue_size; //No need of atomic because we have a mutex
		}
		mutex_unlock(&runqueue_mutex);
	}
	thread_u* tmp = CURRENT_CORE.runqueue;
	LIST_REMOVE(runqueue, CURRENT_CORE.runqueue, CURRENT_CORE.runqueue);
	return tmp;
}
void add_end(int id_core, thread_u* thread)
{
	if(id_core == -1)
		LIST_PREPEND(runqueue, runqueue, thread);
	else
		LIST_PREPEND(runqueue, CURRENT_CORE.runqueue, thread);
}
void add_thread_to_runqueue(int id_core, thread_u * thread)
{
	if(runqueue_size < MAX_SIZE_LOCAL_RUNQUEUE/number_of_core)
		if(mutex_trylock(&runqueue_mutex))
		{
			add_end(-1, thread);
			++runqueue_size;
			mutex_unlock(&runqueue_mutex);
			sem_post(semaphore_runqueue);
			
		}
		else
			add_end(id_core, thread);
	else
		add_end(id_core, thread);
}

void add_begin_thread_to_runqueue(int id_core, thread_u * thread, int priority)
{
	if(priority == MIDDLE_PRIORITY && runqueue_size < MAX_SIZE_LOCAL_RUNQUEUE/number_of_core)
		if(mutex_trylock(&runqueue_mutex))
		{
			LIST_PREPEND(runqueue, runqueue, thread);
			++runqueue_size;
			mutex_unlock(&runqueue_mutex);
			sem_post(semaphore_runqueue);
		}
		else
			LIST_PREPEND(runqueue, CURRENT_CORE.runqueue, thread);
	else
		LIST_PREPEND(runqueue, CURRENT_CORE.runqueue, thread);
}
