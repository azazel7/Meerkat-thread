#include <glib.h>
#include <stdlib.h>
#include "allocator.h"
#include "global.h"
#include "mutex.h"
#include "list.h"

#define COUNT_DIFFERENT_BLOCK 2
static GTrashStack*** trash = NULL;
static mutex_t** trash_mutex = NULL;
extern int number_of_core;

void allocator_init(void)
{
	int i;
	trash = malloc(sizeof(GTrashStack**)*number_of_core);
	for(i = 0; i < number_of_core; ++i)
	{
		trash[i] = malloc(sizeof(GTrashStack*)*COUNT_DIFFERENT_BLOCK);
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
			while((tmp = g_trash_stack_pop(&trash[i][j])) != NULL)
				free(tmp);
		}
		free(trash[i]);
	}
	free(trash);
}
void* allocator_malloc(int id_core, int type)
{
		
	/*mutex_lock(&trash_mutex[type]);*/
	void* chunk = g_trash_stack_pop(&trash[id_core][type]);
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
void allocator_free(int id_core, int type, void* data)
{
	/*mutex_lock(&trash_mutex[type]);*/
	g_trash_stack_push(&trash[id_core][type], data);
	/*mutex_unlock(&trash_mutex[type]);*/
}

