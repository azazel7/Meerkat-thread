#ifndef __GLOBAL_H__
#define __GLOBAL_H__

// The size of the stack of each thread. Work well with 16384. Error if less or equal than 8192
#define SIZE_STACK 16384

typedef struct catch_return {
  void* (*function)(void*);
  void* arg;
} catch_return;

typedef struct thread_u {
  int id;

  // Context for swapcontext, setcontext, ... and everything
  ucontext_t ctx;

  // Stack for the thread
  char stack[SIZE_STACK];

  // Information so the scheduler will know if he must remove the thread
  bool to_clean;

  // Information for the scheduler
  bool is_joining;

  //  To handle one signal
  void* (*signals[10])(int) ;
  catch_return cr;
} thread_u;

#endif __GLOBAL_H__