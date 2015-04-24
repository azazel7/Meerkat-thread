#include <glib.h>
#include <stdlib.h>
#include "allocator.h"
#include "global.h"
#include "mutex.h"
#include "list.h"
#include "htable.h"

static GTrashStack* trash[3] = {NULL};
static mutex_t trash_mutex[3]; 

__attribute__((constructor))
void allocator_init(void)
{
	int i;
	for(i = 0; i < 3; ++i)
		mutex_init(&trash_mutex[i]);
}
__attribute__((destructor))
void allocator_destroy(void)
{
	int i;
	for(i = 0; i < 3; ++i)
	{
		mutex_destroy(&trash_mutex[i]);
		void* chunk = NULL;
		do
		{
			chunk = g_trash_stack_pop(&trash[i]);
			if(chunk != NULL)
				free(chunk);
		} while(chunk != NULL);
	}
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
			case ALLOCATOR_LIST_NODE:
				chunk = malloc(sizeof(struct list_node));
			break;
			case ALLOCATOR_HTABLE_NODE:
				chunk = malloc(sizeof(htable_node));
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

