#include <glib.h>
#include <stdlib.h>
#include "allocator.h"
#include "global.h"
#include "mutex.h"
#include "list.h"
#include "htable.h"

static GTrashStack* **trash = NULL;
static mutex_t trash_mutex[3]; 
extern int number_of_core;

void allocator_init(void)
{
	int i, j;
	for(i = 0; i < 3; ++i)
		mutex_init(&trash_mutex[i]);
	trash = malloc(number_of_core* sizeof(GTrashStack**));
	for(i = 0; i < number_of_core; ++i)
	{
		trash[i] = malloc(3 * sizeof(GTrashStack*));
		for(j = 0; j < 3; ++j)
			trash[i][j] = NULL;
	}
}

__attribute__((destructor))
void allocator_destroy(void)
{
	if(trash == NULL)
		return;
	int i, j;
	for(i = 0; i < 3; ++i)
	{
		mutex_destroy(&trash_mutex[i]);
		for(j = 0; j < number_of_core; ++j)
		{
			void* chunk = NULL;
			do
			{
				chunk = g_trash_stack_pop(&trash[j][i]);
				if(chunk != NULL)
					free(chunk);
			} while(chunk != NULL);
		}
		//XXX memory leak	
	}
	free(trash);
}
void* allocator_malloc(int type)
{
	int id_core = get_idx_core();
	void* chunk = g_trash_stack_pop(&trash[id_core][type]);
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
	int id_core = get_idx_core();
	g_trash_stack_push(&trash[id_core][type], data);
}

