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
#define RUNNNING 0
#define JOINING 1
#define ZOMBI 2
#define OTHER 3
#define GOING_TO_JOIN 4
#define FINISHED 5

typedef struct thread_u
{
	int id;

	//Context for swapcontext, setcontext, ... and everything
	ucontext_t ctx;

	//Stack for the thread
	char* stack;

	//Information for the scheduler
	volatile char state;
	int valgrind_stackid;
	volatile struct thread_u* joiner;
	void* return_value;
	volatile char join_sync;
	LIST_FIELDS(struct thread_u, runqueue);
} thread_u;

typedef struct core_information
{
	pthread_t thread;
	thread_u *current;
	thread_u *previous;
	ucontext_t ctx;
	char stack[SIZE_STACK];
	int valgrind_stackid;
	LIST_HEAD(thread_u, runqueue);
	char* stack_to_free;
} core_information;

int get_idx_core(void);
