#pragma once

typedef struct trash_stack_t
{
	struct trash_stack_t *next;
} trash_stack_t;

static inline void trash_stack_push(trash_stack_t ** stack_p, void *data_p)
{
	((trash_stack_t *) data_p)->next = *stack_p;
	*stack_p = data_p;
}

static inline void *trash_stack_pop(trash_stack_t ** stack_p)
{
	trash_stack_t *data;

	data = *stack_p;
	if(data)
	{
		*stack_p = data->next;
		data->next = NULL;
	}

	return data;
}
