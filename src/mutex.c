#include <stdlib.h>
#include <sched.h>
#include "mutex.h"

#ifndef USE_PTHREAD_MUTEX
bool mutex_init(mutex_t * mutex)
{
	*mutex = malloc(sizeof(bool));
	if(*mutex == NULL)
		return false;
	**mutex = false;
	return true;
}

bool mutex_lock(mutex_t * mutex)
{

	//__sync_lock_test_and_set returns the previous value of mutex, so it should be false if the mutex is not set yet
	int i = 0;
	while(__sync_lock_test_and_set(*mutex, true))
	#ifdef DEBUG
	{
		++i;
		if(i > 20)
		{
			i = 0;
			sched_yield();
		}
	}
	#else
	;
	#endif
	return true;
}

bool mutex_unlock(mutex_t * mutex)
{

	//The mutex isn't locked

	if(**mutex == false)
		return false;
	__sync_lock_release(*mutex);
	return true;
}

void mutex_destroy(mutex_t * mutex)
{
	free(*mutex);
	*mutex = NULL;
}

#endif
