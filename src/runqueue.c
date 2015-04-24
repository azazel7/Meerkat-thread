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
#include "list.h"
#include "htable.h"
#include "runqueue.h"
#include "global.h"
#include "mutex.h"

#define MAX_SIZE_LOCAL_RUNQUEUE 50 

extern List *runqueue;
extern mutex_t runqueue_mutex;
extern sem_t *semaphore_runqueue;
extern core_information *core;
extern int number_of_core;

thread_u *get_thread_from_runqueue(int id_core)
{
	if(list__get_size(core[id_core].runqueue) == 0)
	{
		int i = 0;
		sem_wait(semaphore_runqueue);
		//Lock the runqueue so even if there is many thread in the runqueue, only one will modify the runqueue
		mutex_lock(&runqueue_mutex);
		int to_get = (list__get_size(runqueue) / number_of_core);	
		if(to_get == 0)
			to_get = 1;
		if(to_get > MAX_SIZE_LOCAL_RUNQUEUE)
			to_get = MAX_SIZE_LOCAL_RUNQUEUE;
		for(; i < to_get; ++i)
		{
			list__add_end(CURRENT_CORE.runqueue, list__remove_front(runqueue));
			if(i != 0) //Already done the sem_wait for the first
				sem_wait(semaphore_runqueue);
		}
		mutex_unlock(&runqueue_mutex);
	}
	return list__remove_front(CURRENT_CORE.runqueue);
}

void add_thread_to_runqueue(int id_core, thread_u * thread)
{
	if(list__get_size(runqueue) < MAX_SIZE_LOCAL_RUNQUEUE/number_of_core)
	{
		mutex_lock(&runqueue_mutex);
		list__add_end(runqueue, thread);
		mutex_unlock(&runqueue_mutex);
		sem_post(semaphore_runqueue);
	}
	else
	{
		list__add_end(CURRENT_CORE.runqueue, thread);
	}
}

void add_begin_thread_to_runqueue(int id_core, thread_u * thread)
{
	if(list__get_size(runqueue) < MAX_SIZE_LOCAL_RUNQUEUE/number_of_core)
	{
		mutex_lock(&runqueue_mutex);
		list__add_front(runqueue, thread);
		mutex_unlock(&runqueue_mutex);
		sem_post(semaphore_runqueue);
	}
	else
	{
		list__add_front(CURRENT_CORE.runqueue, thread);
	}
}
