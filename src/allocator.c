#include <glib.h>
#include <stdlib.h>
#include "allocator.h"
#include "global.h"
#include "mutex.h"
#include "list.h"

#define COUNT_DIFFERENT_BLOCK 2
static GTrashStack* trash[COUNT_DIFFERENT_BLOCK] = {NULL};
static mutex_t trash_mutex[COUNT_DIFFERENT_BLOCK];
extern int number_of_core;

void allocator_init(void)
{
	int i;
	for(i = 0; i < COUNT_DIFFERENT_BLOCK; ++i)
		mutex_init(&trash_mutex[i]);
}
__attribute__((destructor))
void allocator_destroy(void)
{
	int i;
	for(i = 0; i < COUNT_DIFFERENT_BLOCK; ++i)
		mutex_destroy(&trash_mutex[i]);
	for(i = 0; i < COUNT_DIFFERENT_BLOCK; ++i)
		while(trash[i] != NULL)
			free(g_trash_stack_pop(&trash[i]));
}
void* allocator_malloc(int type)
{
		
	mutex_lock(&trash_mutex[type]);
	void* chunk = g_trash_stack_pop(&trash[type]);
	mutex_unlock(&trash_mutex[type]);
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
void allocator_free(int type, void* data)
{
	mutex_lock(&trash_mutex[type]);
	g_trash_stack_push(&trash[type], data);
	mutex_unlock(&trash_mutex[type]);
}

