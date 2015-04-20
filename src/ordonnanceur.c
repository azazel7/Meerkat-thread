#include <ucontext.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "list.h"
#include "ordonnanceur.h"
#include "global.h"

#ifndef DEBUG
#define FPRINTF(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define FPRINTF(fmt, ...) do{}while(0)
#endif

// La liste de tous les threads lancé.
extern List* all_thread;

// La liste de tous les threads fini qu'il faut libérer
extern List* all_thread_to_free;

// Un pointeur vers la structure du thread qui est en train de s'exécuter
extern thread_u *current_thread;

// Pour le timer, indique l'interval de temps
extern struct itimerval timeslice;

void thread_schedul() {
  thread_u *previous = current_thread;

  // If the current thread is not finish, we push him back into the runqueu
  if(current_thread->to_clean)
    {

      // Bring back in the runqueu threads that have joined the current_thread
      put_back_joining_thread_of(previous);

      // Do not free here, cause you will free the stack on which you are executed.

      // It's like cut the branch you sit on. It's stupid.
      list__add_front(all_thread_to_free, previous);

      // There is no more previous because we free it
      previous = NULL;
    }
  else if(!current_thread->is_joining)
    {

      // The current thread is joining another one, no need to put it in the runqueu
      list__add_end(all_thread, previous);
    }

  // Get the next thread from the runqueu
  current_thread = list__remove_front(all_thread);
  FPRINTF("Switch from %p to %p\n", previous, current_thread);

  // Reset the timer, eventualy change it
  if(setitimer(ITIMER_REAL, &timeslice, NULL) < 0)
    perror("setitimer");

  // If there is only one thread, no need to change the context, just keep going

  // No need to compare the id, the adress is enough here
  if(previous != NULL && previous != current_thread)
    swapcontext(&(previous->ctx), &(current_thread->ctx));
  else if(previous == NULL && current_thread != NULL)
    // If we destroyed the previous thread, no need so swap, just change the context
    setcontext(&(current_thread->ctx));
  else if(previous == NULL && current_thread == NULL)
    printf("What the fuck ?\n");
  clear_finished_thread();
}