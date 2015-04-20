#pragma once

//The size of the stack of each thread. Work well with 16384. Error if less or equal than 8192
#define SIZE_STACK 16384

typedef struct catch_return
{
	void *(*function) (void *);
	void *arg;
} catch_return;
typedef struct thread_u
{
	int id;

	//Context for swapcontext, setcontext, ... and everything
	ucontext_t ctx;

	//Stack for the thread
	char stack[SIZE_STACK];

	//Information so the scheduler will know if he must remove the thread
	bool to_clean;

	//Information for the scheduler
	bool is_joining;
	int valgrind_stackid;
	catch_return cr;
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
} core_information;

