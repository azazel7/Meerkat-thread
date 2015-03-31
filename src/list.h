#pragma once
#include <stdbool.h>

typedef struct list_node
{
	void* data;
	struct list_node* next, *previous;
} List_node;
typedef struct list
{
	List_node* head, *iterator;
	unsigned int size;
	bool begin_is_to_see;
} List;

List* list__create(void);
void list__init_iterator(List* list);
/*
 * Return true if there is an element after the current iterator
 */
bool list__has_next_iterator(List* list);
//Add an element at the front of the list
void list__add_front(List* list, void* data);
//Add an element at the end of the list
void list__add_end(List* list, void* data);
//Remove an element at the front of the list
void* list__remove_front(List* list);
//Remove an element at the end of the list
void* list__remove_end(List* list);
void* list__next_iterator(List* list);
void* list__top(List* list);
void* list__remove_on_for_each(List* list);
void* list__remove(List* list);
void list__append(List* dest, List* to_add);
void list__clear(List* list);
void list__destroy(List* list);
bool list__is_end_iterator(List* list);
unsigned int list__get_size(List* list);
void list__apply(List* list, void (*func)()); 
static void* list__remove_iterator(List* list, bool on_for_each);
#define list__for_each(l, i) \
	for (i = list__top(l), list__init_iterator(l); \
	     list__is_end_iterator(l) == false; \
	     i = list__next_iterator(l))

