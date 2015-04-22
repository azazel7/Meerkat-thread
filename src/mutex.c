#include <stdlib.h>
#include <stdio.h>
#include "mutex.h"

#ifndef USE_PTHREAD_MUTEX
bool mutex_init(mutex_t* mutex) {
  fprintf(stdout, "init our_mutex\n") ;
  *mutex = malloc(sizeof(bool));
  if (*mutex == NULL)
    return false;
  **mutex = false;
  return true;
}

bool mutex_lock(mutex_t* mutex) {
  
  //__sync_lock_test_and_set returns the previous value of mutex, so it should be false if the mutex is not set yet
  
  while (__sync_lock_test_and_set(*mutex, true));
  return true ;
}

bool mutex_unlock(mutex_t* mutex) {
  
  //The mutex isn't locked

  if (**mutex == false)
    return false;
  __sync_lock_release(*mutex);
  return true;
}

void mutex_destroy(mutex_t* mutex) {

  fprintf(stdout, "destroy our_mutex\n") ;
  
  free(*mutex);
  *mutex = NULL;
}

#endif
