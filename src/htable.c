#include <stdio.h>
#include <stdlib.h>
#include "htable.h"
#include "allocator.h"

static int htable__hash_int(void *);
static bool htable__cmp_int(void *, void *);

htable *htable__create_int(bool use_lock)
{
	return htable__create(htable__hash_int, htable__cmp_int, use_lock);
}

htable *htable__create(int (*hash) (), bool(*cmp) (), bool use_lock)
{
	htable *h_table = malloc(sizeof(htable));
	if(h_table == NULL)
	{
		fprintf(stderr, "Memory out of stock\n");
		exit(EXIT_FAILURE);
	}
	h_table->cmp = cmp;
	h_table->hash = hash;
	h_table->size = 0;
	h_table->use_lock = use_lock;
	int i;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
	{
		h_table->table[i] = list__create();
		if(h_table->table[i] == NULL)
		{
			fprintf(stderr, "Memory out of stock\n");
			exit(EXIT_FAILURE);
		}
	}
	if(use_lock)
		for(i = 0; i < SIZE_HASH_TABLE; i++)
			if(!mutex_init(&h_table->locks[i]))
			{
				perror("htable_create -> mutex_init");
				exit(EXIT_FAILURE);
			}

	return h_table;
}

bool htable__contain(htable * h_table, void *key)
{
	int idx = h_table->hash(key);
	htable_node *node = NULL;
	if(h_table->use_lock)
		mutex_lock(&(h_table->locks[idx]));
	list__for_each(h_table->table[idx], node)
	{
		if(h_table->cmp(node->key, key))
		{
			if(h_table->use_lock)
				mutex_unlock(&(h_table->locks[idx]));
			return true;
		}
	}
	if(h_table->use_lock)
		mutex_unlock(&h_table->locks[idx]);
	return false;
}

void htable__insert(htable * h_table, void *key, void *data)
{
	int idx = h_table->hash(key);
	htable_node *node = allocator_malloc(ALLOCATOR_HTABLE_NODE);
	if(node == NULL)
	{
		fprintf(stderr, "Memory out of stock\n");
		exit(EXIT_FAILURE);
	}
	node->key = key;
	node->data = data;
	if(h_table->use_lock)
		mutex_lock(&h_table->locks[idx]);
	list__add_front(h_table->table[idx], node);
	if(h_table->use_lock)
		mutex_unlock(&(h_table->locks[idx]));
	if(h_table->use_lock)
		__sync_add_and_fetch(&h_table->size, 1);
	else
		h_table->size++;
}

void* htable__remove(htable * h_table, void *key)
{
	int idx = h_table->hash(key);
	List *list = h_table->table[idx];
	htable_node *node = NULL;
	bool found = false;
	if(h_table->use_lock)
		mutex_lock(&(h_table->locks[idx]));
	void* tmp = NULL;
	list__for_each(list, node)
	{
		if(h_table->cmp(node->key, key))
		{
			list__remove(list);
			tmp = node->data;
			found = true;
			if(h_table->use_lock)
				__sync_sub_and_fetch(&h_table->size, 1);
			else
				--h_table->size;
			break;
		}
	}
	if(h_table->use_lock)
		mutex_unlock(&(h_table->locks[idx]));
	if(found)
		allocator_free(ALLOCATOR_HTABLE_NODE, node);
	return tmp;
}

int htable__size(htable * h_table)
{
	return h_table->size;
}

bool htable__get_element(htable * h_table, void **key, void **data)
{
	if(h_table->size == 0)
		return NULL;
	int i;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
	{
		if(h_table->use_lock)
			mutex_lock(&(h_table->locks[i]));
		if(list__get_size(h_table->table[i]) > 0)
		{
			htable_node *node = list__top(h_table->table[i]);
			if(key != NULL)
				*key = node->key;
			if(data != NULL)
				*data = node->data;
			if(h_table->use_lock)
				mutex_unlock(&(h_table->locks[i]));
			return true;
		}
		if(h_table->use_lock)
			mutex_unlock(&(h_table->locks[i]));
	}
	return false;
}

void htable__apply(htable * h_table, void (*function) (void *))
{
	int i;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
	{
		htable_node *node = NULL;
		if(h_table->use_lock)
			mutex_lock(&(h_table->locks[i]));
		list__for_each(h_table->table[i], node) function(node->data);
		if(h_table->use_lock)
			mutex_unlock(&(h_table->locks[i]));
	}
}

void *htable__find(htable * h_table, void *key)
{
	int idx = h_table->hash(key);
	htable_node *node = NULL;
	if(h_table->use_lock)
		mutex_lock(&(h_table->locks[idx]));

	list__for_each(h_table->table[idx], node)
	{
		if(h_table->cmp(node->key, key))
		{
			if(h_table->use_lock)
				mutex_unlock(&(h_table->locks[idx]));
			return node->data;
		}
	}
	if(h_table->use_lock)
		mutex_unlock(&(h_table->locks[idx]));
	return NULL;
}

void* htable__find_and_apply(htable* h_table, void* key, void(*f)(void*))
{
	int idx = h_table->hash(key);
	htable_node *node = NULL;
	if(h_table->use_lock)
		mutex_lock(&(h_table->locks[idx]));

	list__for_each(h_table->table[idx], node)
	{
		if(h_table->cmp(node->key, key))
		{
			f(node->data);
			if(h_table->use_lock)
				mutex_unlock(&(h_table->locks[idx]));
			return node->data;
		}
	}
	if(h_table->use_lock)
		mutex_unlock(&(h_table->locks[idx]));
	return NULL;
}
 
void htable__destroy(htable * h_table)
{
	int i;
	htable_node *node = NULL;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
	{
		if(h_table->use_lock)
			mutex_lock(&(h_table->locks[i]));
		list__for_each(h_table->table[i], node) allocator_free(ALLOCATOR_HTABLE_NODE, node);
		list__destroy(h_table->table[i]);
		if(h_table->use_lock)
			mutex_unlock(&(h_table->locks[i]));
	}
	if(h_table->use_lock)
		for(i = 0; i < SIZE_HASH_TABLE; i++)
			mutex_destroy(&h_table->locks[i]);
	free(h_table);
}

int htable__hash_int(void *data)
{
	return ((long int)data) % SIZE_HASH_TABLE;
}

bool htable__cmp_int(void *d1, void *d2)
{
	return (long int)d1 == (long int)d2;
}
