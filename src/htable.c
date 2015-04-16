#include <stdio.h>
#include <stdlib.h>
#include "htable.h"
#include "list.h"

typedef struct node
{
	void* data;
	void* key;
} htable_node;

struct htable
{
	List *table[SIZE_HASH_TABLE];
	int (*hash) ();
	bool(*cmp) ();
	int size;
};
typedef struct htable htable;

static int htable__hash_int(void*);
static bool htable__cmp_int(void*, void*);

htable* htable__create_int(void)
{
	return htable__create(htable__hash_int, htable__cmp_int);
}
htable *htable__create(int (*hash) (), bool(*cmp) ())
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
	return h_table;
}

bool htable__contain(htable * h_table, void *key)
{
	int idx = h_table->hash(key);
	htable_node* node = NULL;
	list__for_each(h_table->table[idx], node)
	{
		if(h_table->cmp(node->key, key))
			return true;
	}
	return false;
}

void htable__insert(htable *h_table, void* key, void *data)
{
	int idx = h_table->hash(key);
	htable_node* node = malloc(sizeof(htable_node));
	if(node == NULL)
	{
		fprintf(stderr, "Memory out of stock\n");
		exit(EXIT_FAILURE);
	}
	node->key = key;
	node->data = data;
	list__add_front(h_table->table[idx], node);
	h_table->size++;
}

void htable__remove(htable *h_table, void *key)
{
	int idx = h_table->hash(key);
	List *list = h_table->table[idx];
	htable_node* node = NULL;
	list__for_each(list, node)
	{
		if(h_table->cmp(node->key, key))
		{
			list__remove(list);
			free(node);
			--h_table->size;
			break;
		}
	}
}

int htable__size(htable *h_table)
{
	return h_table->size;
}

void htable__merge(htable *h_table, htable *to_add)
{
	List *current;
	htable_node* node = NULL;
	int i;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
	{
		current = to_add->table[i];
		list__for_each(current, node)
		{
				if(!htable__contain(h_table, node->key))
					htable__insert(h_table, node->key, node->data);
		}
	}
}

bool htable__get_element(htable* h_table, void ** key, void** data)
{
	if(h_table->size == 0)
		return NULL;
	int i;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
		if(list__get_size(h_table->table[i]) > 0)
		{
			htable_node* node = list__top(h_table->table[i]);
			if(key != NULL)
				*key = node->key;
			if(data != NULL)
				*data = node->data;
			return true;
		}
	return false;
}

void htable__apply(htable *h_table, void (*function) (void *))
{
	int i;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
	{
		htable_node* node = NULL;
		list__for_each(h_table->table[i], node)
			function(node->data);
	}
}

void *htable__find(htable *h_table, void *key)
{
	int idx = h_table->hash(key);
	htable_node* node = NULL;
	list__for_each(h_table->table[idx], node)
	{
		if(h_table->cmp(node->key, key))
			return node->data;
	}
	return NULL;
}

void htable__destroy(htable *h_table)
{
	int i;
	htable_node* node = NULL;
	for(i = 0; i < SIZE_HASH_TABLE; i++)
	{
		list__for_each(h_table->table[i], node)
			free(node);
		list__destroy(h_table->table[i]);
	}
	free(h_table);
}

int htable__hash_int(void* data)
{
	return ((long int) data) % SIZE_HASH_TABLE;
}
bool htable__cmp_int(void* d1, void* d2)
{
	return (long int)d1 == (long int) d2;
}
