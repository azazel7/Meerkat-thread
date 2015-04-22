#include <stdbool.h>

#ifndef USE_PTHREAD_MUTEX

typedef bool* mutex_t;

bool mutex_init(mutex_t* mutex);
bool mutex_lock(mutex_t* mutex);
bool mutex_unlock(mutex_t* mutex);
void mutex_destroy(mutex_t* mutex);


#else /* USE_PTHREAD_MUTEX */

#include <pthread.h>
#include <stdlib.h>
#define mutex_t pthread_mutex_t
#define mutex_init(mut) pthread_mutex_init((mut), NULL)
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock
#define mutex_destroy pthread_mutex_destroy


#endif /* USE_PTHREAD_MUTEX */

