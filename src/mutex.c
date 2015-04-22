#include <stdlib.h>
#include "mutex.h"


bool our_mutex__init(our_mutex_t* mutex) {
  *mutex = malloc(sizeof(bool));
  if (*mutex == NULL)
    return false;
  **mutex = false;
  return true;
}

bool our_mutex__lock(our_mutex_t mutex) {
  
  //__sync_lock_test_and_set returns the previous value of mutex, so it should be false if the mutex is not set yet
  
  while (__sync_lock_test_and_set(mutex, true));
  return true ;
}

bool our_mutex__unlock(our_mutex_t mutex) {
  
  //The mutex isn't locked

  if (*mutex == false)
    return false;
  __sync_lock_release(mutex);
  return true;
}

void our_mutex__destroy(our_mutex_t* mutex) {
  free(*mutex);
  *mutex = NULL;
}
