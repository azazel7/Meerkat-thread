#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
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
unsigned int runqueue_size = 0;

static void fill_local_runqueue(int const id_core)
{
		int i = 0;
		//Compute how many thread we are going to take
		int to_get = (runqueue_size / NUMBER_OF_CORE);	
		if(to_get > MAX_SIZE_LOCAL_RUNQUEUE) //Don't take too many
			to_get = MAX_SIZE_LOCAL_RUNQUEUE;
		//Get
		for(; i < to_get; ++i)
			if(sem_trywait(semaphore_runqueue) != 0)
				break;
		to_get = i + 1;	
		runqueue_size -= to_get;
		
		thread_u* tmp;
		assert(runqueue);
		LIST_GET_IEME(runqueue, runqueue, tmp, to_get);
		LIST_CUT_AFTER_AND_PUT_IN_FRONT(runqueue, runqueue, tmp, CURRENT_CORE.runqueue);
		assert(CURRENT_CORE.runqueue);
}

thread_u *get_thread_from_runqueue(int const id_core)
{
	//If nothing is in our queue
	if(CURRENT_CORE.runqueue == NULL)
	{
		//Wait for something in the global runqueue
		sem_wait(semaphore_runqueue);
		//Lock the runqueue so even if there is many thread in the runqueue, only one will modify the runqueue
		mutex_lock(&runqueue_mutex);
		
		fill_local_runqueue(id_core);
		//Release the lock
		mutex_unlock(&runqueue_mutex);
	}
	
	//Remove from the top of the local runqueue
	thread_u* tmp = CURRENT_CORE.runqueue;
	LIST_REMOVE(runqueue, CURRENT_CORE.runqueue, CURRENT_CORE.runqueue);
	return tmp;
}

thread_u* try_get_thread_from_runqueue(int const id_core)
{
	//If nothing is in our queue
	if(CURRENT_CORE.runqueue == NULL)
	{
		if(sem_trywait(semaphore_runqueue) != 0)
			return NULL;
		
		assert(runqueue);
		//Lock the runqueue so even if there is many thread in the runqueue, only one will modify the runqueue
		mutex_lock(&runqueue_mutex);
		
		fill_local_runqueue(id_core);
		
		//Release the lock
		mutex_unlock(&runqueue_mutex);
	}
	
	//Remove from the top of the local runqueue
	thread_u* tmp = CURRENT_CORE.runqueue;
	LIST_REMOVE(runqueue, CURRENT_CORE.runqueue, CURRENT_CORE.runqueue);
	return tmp;
}

void add_begin_thread_to_runqueue(int const id_core, thread_u * const thread, int const priority)
{
	//If the thread doesn't have to be executed right now and the global runqueue is empty, we will wait to put it into the global runqueue with a mutex
	if(priority == MIDDLE_PRIORITY && runqueue_size < MAX_SIZE_LOCAL_RUNQUEUE/NUMBER_OF_CORE)
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
