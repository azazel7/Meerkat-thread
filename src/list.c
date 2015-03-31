#include "list.h"
#include <stdlib.h>

void list__init_iterator(List* list)
{
	list->iterator = list->head;
}
void list__add_front(List* list, void* data)
{
	List_node* new_node = malloc(sizeof(List_node));
	if(new_node == NULL)
		return;
	new_node->data = data;
	new_node->next = list->head;
	new_node->previous = NULL;
	if(list->head != NULL)
		list->head->previous = new_node;
	list->head = new_node;
	++list->size;
}
void* list__remove_front(List* list)
{
	if(list->size == 0)
		return NULL;
	void* tmp = list->head->data;
	List_node* to_free = list->head;
	list->head = list->head->next;
	if(list->head != NULL)
		list->head->previous = NULL;
	//If the iterator was on the head, put it on the new head
	if(list->iterator == to_free)
		list->iterator = list->head;
	free(to_free);
	--list->size;
	return tmp;
}
bool list__has_next_iterator(List* list)
{
	if(list->iterator == NULL)
		return false;
	if(list->begin_is_to_see)
	{
		list->begin_is_to_see = false;//Can't call this function many time
		return true;
	}
	if(list->iterator->next == NULL)
		return false;
	return true;
}
bool list__is_end_iterator(List* list)
{
	return list->iterator == NULL;
}
void* list__next_iterator(List* list)
{
	if(list->iterator == NULL)
		return NULL;
	if(list->begin_is_to_see)
	{
		list->begin_is_to_see = false;
		return list->iterator->data;
	}
	list->iterator = list->iterator->next;
	if(list->iterator == NULL)
		return NULL;
	return list->iterator->data;
}
void* list__top(List* list)
{
	if(list->size == 0)
		return NULL;
	return list->head->data;
}
void* list__remove(List* list)
{
	return list__remove_iterator(list, false);
}
void list__clear(List* list)
{
	while(list->size > 0)
		list__remove_front(list);
}
List* list__create(void)
{
	List* ret = malloc(sizeof(List));
	if(ret == NULL)
		return NULL;
	ret->size = 0;
	ret->head = ret->iterator = NULL;
	ret->begin_is_to_see = false;
	return ret;
}
void list__destroy(List* list)
{
	while(list->head != NULL)
	{
		List_node* tmp = list->head;
		list->head = list->head->next;
		free(tmp);
	}
	free(list);
}
void* list__remove_on_for_each(List* list)
{
	//remove_on_for_each -> add stuff to bypass the way has_next_iterator and next_iterator work
	return list__remove_iterator(list, true);
}
static void* list__remove_iterator(List* list, bool on_for_each)
{
	void* tmp = NULL;
	if(list->iterator == NULL || list->size == 0)
		return NULL;
	if(list->iterator->previous == NULL) //Removing the first
	{
		tmp = list__remove_front(list);
		if(on_for_each)
			list->begin_is_to_see = true;
		return tmp;
	}

	List_node* to_free = list->iterator, *prev, *next;
	tmp = to_free->data;
	prev = to_free->previous;
	next = to_free->next;
	//Change the links
	prev->next = next;
	if(next != NULL)
		next->previous = prev;
	free(to_free);
	//Place the next iterator
	list->iterator = prev;
	--list->size;
	return tmp;
}
void list__append(List* dest, List* to_add)
{
	void* data= NULL;
	//Iterate on to_add
	list__for_each(to_add, data)
	{
		//And add at the end of dest
		list__add_end(dest, data);
	}
}
void list__add_end(List* list, void* data)
{
	if(list->size == 0)
	{
		list__add_front(list, data);
		return;
	}
	if(list->iterator == NULL)
		list->iterator = list->head;
	while(list->iterator->next != NULL)
	{
		list->iterator = list->iterator->next;
	}
	List_node* new_node = malloc(sizeof(List_node));
	if(new_node == NULL)
		return;
	new_node->data = data;
	new_node->next = NULL;
	new_node->previous = list->iterator;
	list->iterator->next = new_node;
	++list->size;
}
unsigned int list__get_size(List* list)
{
	return list->size;
}
void* list__remove_end(List* list)
{
	if(list->size == 0)
		return NULL;
	List_node* tmp = list->head;
	while(tmp->next != NULL)
		tmp = tmp->next;
	void* ret = tmp->data;
	if(tmp->previous != NULL)
		tmp->previous->next = NULL;
	else
		list->head = NULL;
	free(tmp);
	--list->size;
	return ret;
}
void list__apply(List* list, void (*func)())
{
	void* tmp = NULL;
	list__for_each(list, tmp)
	{
		func(tmp);
	}
}
