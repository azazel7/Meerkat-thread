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


extern List *runqueue;
extern mutex_t runqueue_mutex;
extern sem_t *semaphore_runqueue;

thread_u * get_thread_from_runqueue(){
	//Lock the runqueue so even if there is many thread in the runqueue, only one will modify the runqueue
	mutex_lock(&runqueue_mutex);
	thread_u * thread = list__remove_front(runqueue);
	mutex_unlock(&runqueue_mutex);
	return thread;
}
void add_thread_to_runqueue(thread_u* thread)
{
		mutex_lock(&runqueue_mutex);
		list__add_end(runqueue, thread);
		mutex_unlock(&runqueue_mutex);
		sem_post(semaphore_runqueue);
}
