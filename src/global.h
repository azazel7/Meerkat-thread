#pragma once
#include <ucontext.h>
#include <stdbool.h>
#include "list.h"

//The size of the stack of each thread. Work well with 16384. Error if less or equal than 8192
#define SIZE_STACK 16384

#define CURRENT_CORE core[id_core]
#define CURRENT_THREAD core[id_core].current
#define IGNORE_SIGNAL(i) signal(i, empty_handler)
#define UNIGNORE_SIGNAL(i) signal(i, thread_handler)

#ifdef DEBUG
#define FPRINTF(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define FPRINTF(fmt, ...) do{}while(0)
#endif

typedef struct thread_u
{
	int id;

	//Context for swapcontext, setcontext, ... and everything
	ucontext_t ctx;

	//Stack for the thread
	char* stack;

	//Information so the scheduler will know if he must remove the thread
	bool to_clean;

	//Information for the scheduler
	bool is_joining;
	int valgrind_stackid;
	struct thread_u* joiner;
	int id_joining;
} thread_u;
typedef struct core_information
{
	pthread_t thread;
	thread_u *current;
	thread_u *previous;
	ucontext_t ctx;
	char stack[SIZE_STACK];
	bool unlock_runqueue;
	bool unlock_join_queue;
	int valgrind_stackid;
	List* runqueue;
} core_information;

int get_idx_core(void);
