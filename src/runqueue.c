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


extern List *runqueue;
extern pthread_spinlock_t runqueue_mutex;

thread_u * get_thread_from_runqueue(){
	//Lock the runqueue so even if there is many thread in the runqueue, only one will modify the runqueue
	pthread_spin_lock(&runqueue_mutex);
	thread_u * thread = list__remove_front(runqueue);
	pthread_spin_unlock(&runqueue_mutex);
	return thread;
}
