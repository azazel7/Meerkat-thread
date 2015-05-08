#include <stdlib.h>
#include "allocator.h"
#include "global.h"
#include "mutex.h"
#include "list.h"
#include "trash_stack.h"

#define COUNT_DIFFERENT_BLOCK 2
static trash_stack_t*** trash = NULL;
/*static mutex_t** trash_mutex = NULL;*/
extern int number_of_core;

void allocator_init(void)
{
	int i;
	trash = malloc(sizeof(trash_stack_t**)*number_of_core);
	for(i = 0; i < number_of_core; ++i)
	{
		trash[i] = malloc(sizeof(trash_stack_t*)*COUNT_DIFFERENT_BLOCK);
		int j;
		for(j = 0; j < COUNT_DIFFERENT_BLOCK; ++j)
		{
			trash[i][j] = NULL;
		}
	}
	/*for(i = 0; i < COUNT_DIFFERENT_BLOCK; ++i)*/
		/*mutex_init(&trash_mutex[i]);*/
}
__attribute__((destructor))
void allocator_destroy(void)
{
	int i;
	void* tmp;
	/*for(i = 0; i < COUNT_DIFFERENT_BLOCK; ++i)*/
		/*mutex_destroy(&trash_mutex[i]);*/
	for(i = 0; i < number_of_core; ++i)
	{
		int j;
		for(j = 0; j < COUNT_DIFFERENT_BLOCK; ++j)
		{
			while((tmp = trash_stack_pop(&trash[i][j])) != NULL)
				free(tmp);
		}
		free(trash[i]);
	}
	free(trash);
}
void* allocator_malloc(int const id_core, int const type)
{
	/*mutex_lock(&trash_mutex[type]);*/
	void* chunk = trash_stack_pop(&trash[id_core][type]);
	/*mutex_unlock(&trash_mutex[type]);*/
	if(chunk == NULL)
	{
		switch(type)
		{
			case ALLOCATOR_THREAD:
				chunk = malloc(sizeof(thread_u));
			break;
			case ALLOCATOR_STACK:
				chunk = malloc(SIZE_STACK);
			break;
		}
	}
	return chunk;
}
void allocator_free(int const id_core, int const type, void* const data)
{
	/*mutex_lock(&trash_mutex[type]);*/
	trash_stack_push(&trash[id_core][type], data);
	/*mutex_unlock(&trash_mutex[type]);*/
}

