#include <stdlib.h>
#include "ctest.h"
#include "../src/list.h"


CTEST(list, add_front)
{
	List* list = list__create();
	int a = 1, b = 2;
	list__add_front(list, &a);
	ASSERT_EQUAL(1, list__get_size(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(2, list__get_size(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(3, list__get_size(list));
	list__destroy(list);
}

CTEST(list, top)
{
	List* list = list__create();
	int a = 1, b = 2;
	ASSERT_NULL(list__top(list));
	list__add_front(list, &a);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(1, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(3, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	list__destroy(list);
}

CTEST(list, remove_front)
{
	List* list = list__create();
	int a = 1, b = 2;
	list__add_front(list, &a);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(1, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(3, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	int* dat = list__remove_front(list);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	ASSERT_EQUAL(2, *dat);
	dat = list__remove_front(list);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(1, *(int*)list__top(list));
	ASSERT_EQUAL(2, *dat);
	dat = list__remove_front(list);
	ASSERT_EQUAL(0, list__get_size(list));
	ASSERT_NULL((int*)list__top(list));
	ASSERT_EQUAL(1, *dat);
	list__destroy(list);
}

CTEST(list, remove_end)
{
	List* list = list__create();
	int a = 1, b = 2;
	list__add_front(list, &a);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(1, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(3, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	int* dat = list__remove_end(list);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	ASSERT_EQUAL(1, *dat);
	dat = list__remove_end(list);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	ASSERT_EQUAL(2, *dat);
	dat = list__remove_end(list);
	ASSERT_EQUAL(0, list__get_size(list));
	ASSERT_NULL((int*)list__top(list));
	ASSERT_EQUAL(2, *dat);
	list__destroy(list);
}

CTEST(list, clear)
{
	List* list = list__create();
	int a = 1, b = 2;
	list__add_front(list, &a);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(1, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	list__add_front(list, &b);
	ASSERT_EQUAL(3, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	list__clear(list);
	ASSERT_EQUAL(0, list__get_size(list));
	list__destroy(list);
}

CTEST(list, add_end)
{
	List* list = list__create();
	int a = 1, b = 2;
	list__add_end(list, &a);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(1, *(int*)list__top(list));
	list__add_end(list, &b);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(1, *(int*)list__top(list));
	list__remove_front(list);
	ASSERT_EQUAL(1, list__get_size(list));
	ASSERT_EQUAL(2, *(int*)list__top(list));
	list__destroy(list);
}

CTEST(list, has_next_iterator)
{
	List* list = list__create();
	int a = 1;
	list__init_iterator(list);
	ASSERT_FALSE(list__has_next_iterator(list));
	list__add_front(list, &a);
	list__init_iterator(list);
	ASSERT_FALSE(list__has_next_iterator(list));//Because there is only one element, so no more element after
	list__destroy(list);
}

CTEST(list, has_next_iterator2)
{
	List* list = list__create();
	int a = 1, b = 2;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__init_iterator(list);
	ASSERT_TRUE(list__has_next_iterator(list));
	list__next_iterator(list);
	ASSERT_FALSE(list__has_next_iterator(list));
	list__destroy(list);
}

CTEST(list, next_iterator)
{
	List* list = list__create();
	int a = 1, b = 2;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__init_iterator(list);
	ASSERT_TRUE(list__has_next_iterator(list));
	int* tmp = list__next_iterator(list);
	ASSERT_EQUAL(*tmp, a);
	list__destroy(list);
}

CTEST(list, for_each)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__for_each(list, tmp)
	{
		if(i == 0)
			ASSERT_EQUAL(c, *tmp);
		else if(i == 1)
			ASSERT_EQUAL(b, *tmp);
		else if(i == 2)
			ASSERT_EQUAL(a, *tmp);
		++i;
	}
	ASSERT_EQUAL(3, i);
	list__destroy(list);
}

CTEST(list, for_each2)
{
	List* list = list__create();
	int a = 1, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__for_each(list, tmp)
	{
		if(i == 0)
			ASSERT_EQUAL(a, *tmp);
		++i;
	}
	ASSERT_EQUAL(1, i);
	list__destroy(list);
}

CTEST(list, for_each3)
{
	List* list = list__create();
	int i = 0;
	int* tmp = NULL;
	list__for_each(list, tmp)
	{
		++i;
	}
	ASSERT_EQUAL(0, i);
	list__destroy(list);
}

CTEST(list, for_each4)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__for_each(list, tmp)
		break;
	list__for_each(list, tmp)
	{
		if(i == 0)
			ASSERT_EQUAL(c, *tmp);
		else if(i == 1)
			ASSERT_EQUAL(b, *tmp);
		else if(i == 2)
			ASSERT_EQUAL(a, *tmp);
		++i;
	}
	ASSERT_EQUAL(3, i);
	list__destroy(list);
}

CTEST(list, remove_on_for_each)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__for_each(list, tmp)
	{
		if(i == 0)
			ASSERT_EQUAL(c, *tmp);
		else if(i == 1)
		{
			ASSERT_EQUAL(b, *tmp);
			list__remove_on_for_each(list);
		}
		else if(i == 2)
			ASSERT_EQUAL(a, *tmp);
		++i;
	}
	ASSERT_EQUAL(3, i);
	ASSERT_EQUAL(2, list__get_size(list));
	list__destroy(list);
}

CTEST(list, remove_on_for_each2)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__for_each(list, tmp)
	{
		if(i == 0)
		{
			ASSERT_EQUAL(c, *tmp);
			list__remove_on_for_each(list);
		}
		else if(i == 1)
			ASSERT_EQUAL(b, *tmp);
		else if(i == 2)
			ASSERT_EQUAL(a, *tmp);
		++i;
	}
	ASSERT_EQUAL(3, i);
	ASSERT_EQUAL(2, list__get_size(list));
	list__destroy(list);
}

CTEST(list, remove_on_for_each3)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__for_each(list, tmp)
	{
		if(i == 0)
			ASSERT_EQUAL(c, *tmp);
		else if(i == 1)
			ASSERT_EQUAL(b, *tmp);
		else if(i == 2)
		{
			ASSERT_EQUAL(a, *tmp);
			list__remove_on_for_each(list);
		}
		++i;
	}
	ASSERT_EQUAL(3, i);
	ASSERT_EQUAL(2, list__get_size(list));
	list__destroy(list);
}

CTEST(list, remove_on_for_each4)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__for_each(list, tmp)
	{
		if(i == 0)
		{
			ASSERT_EQUAL(c, *tmp);
			list__remove_on_for_each(list);
		}
		else if(i == 1)
			ASSERT_EQUAL(b, *tmp);
		else if(i == 2)
		{
			ASSERT_EQUAL(a, *tmp);
			list__remove_on_for_each(list);
		}
		++i;
	}
	ASSERT_EQUAL(3, i);
	ASSERT_EQUAL(1, list__get_size(list));
	list__destroy(list);
}

CTEST(list, remove_on_for_each5)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3, i = 0;
	int* tmp = NULL;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__for_each(list, tmp)
	{
		if(i == 0)
			ASSERT_EQUAL(c, *tmp);
		else if(i == 1)
			ASSERT_EQUAL(b, *tmp);
		else if(i == 2)
			ASSERT_EQUAL(a, *tmp);
		list__remove_on_for_each(list);
		++i;
	}
	ASSERT_EQUAL(3, i);
	ASSERT_EQUAL(0, list__get_size(list));
	list__destroy(list);
}

CTEST(list, remove)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	ASSERT_EQUAL(3, list__get_size(list));
	list__init_iterator(list);
	ASSERT_EQUAL(c, *(int*)list__top(list));
	list__remove(list);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(b, *(int*)list__top(list));
	list__destroy(list);
}

CTEST(list, remove2)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	ASSERT_EQUAL(3, list__get_size(list));
	list__init_iterator(list);
	ASSERT_EQUAL(c, *(int*)list__top(list));
	int* tmp = list__next_iterator(list);
	int* tmp2 = list__remove(list);
	int* tmp3 = list__next_iterator(list);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(b, *tmp);
	ASSERT_EQUAL(b, *tmp2);
	ASSERT_EQUAL(a, *tmp3);
	list__destroy(list);
}

CTEST(list, remove3)
{
	List* list = list__create();
	int a = 1, b = 2, c = 3;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	ASSERT_EQUAL(3, list__get_size(list));
	list__init_iterator(list);
	ASSERT_EQUAL(c, *(int*)list__top(list));
	list__next_iterator(list);
	int* tmp = list__next_iterator(list);
	int* tmp2 = list__remove(list);
	int* tmp3 = list__next_iterator(list);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(a, *tmp);
	ASSERT_EQUAL(a, *tmp2);
	ASSERT_NULL(tmp3);
	list__destroy(list);
}

CTEST(list, append)
{
	List* list = list__create();
	List* list2 = list__create();
	int a = 1, b = 2, c = 3;
	list__add_front(list, &a);
	list__add_front(list, &b);
	list__add_front(list, &c);
	list__add_front(list2, &a);
	list__add_front(list2, &b);
	ASSERT_EQUAL(3, list__get_size(list));
	ASSERT_EQUAL(2, list__get_size(list2));
	list__append(list, list2);
	ASSERT_EQUAL(5, list__get_size(list));
	ASSERT_EQUAL(2, list__get_size(list2));
	list__init_iterator(list);
	int* tmp0 = list__top(list);
	int* tmp1 = list__next_iterator(list);
	int* tmp2 = list__next_iterator(list);
	int* tmp3 = list__next_iterator(list);
	int* tmp4 = list__next_iterator(list);
	
	ASSERT_NOT_NULL(tmp0);
	ASSERT_NOT_NULL(tmp1);
	ASSERT_NOT_NULL(tmp2);
	ASSERT_NOT_NULL(tmp3);
	ASSERT_NOT_NULL(tmp4);
	ASSERT_EQUAL(3, *tmp0);
	ASSERT_EQUAL(2, *tmp1);
	ASSERT_EQUAL(1, *tmp2);
	ASSERT_EQUAL(2, *tmp3);
	ASSERT_EQUAL(1, *tmp4);
	list__destroy(list);
	list__destroy(list2);
}

CTEST(list, append2)
{
	List* list = list__create();
	List* list2 = list__create();
	int a = 1, b = 2;
	list__add_front(list2, &a);
	list__add_front(list2, &b);
	ASSERT_EQUAL(0, list__get_size(list));
	ASSERT_EQUAL(2, list__get_size(list2));
	list__append(list, list2);
	ASSERT_EQUAL(2, list__get_size(list));
	ASSERT_EQUAL(2, list__get_size(list2));
	list__init_iterator(list);
	int* tmp0 = list__top(list);
	int* tmp1 = list__next_iterator(list);
	ASSERT_NOT_NULL(tmp0);
	ASSERT_NOT_NULL(tmp1);
	ASSERT_EQUAL(2, *tmp0);
	ASSERT_EQUAL(1, *tmp1);
	list__destroy(list);
	list__destroy(list2);
}

CTEST(list, append3)
{
	List* list = list__create();
	List* list2 = list__create();
	ASSERT_EQUAL(0, list__get_size(list));
	ASSERT_EQUAL(0, list__get_size(list2));
	list__append(list, list2);
	ASSERT_EQUAL(0, list__get_size(list));
	ASSERT_EQUAL(0, list__get_size(list2));
	list__init_iterator(list);
	int* tmp0 = list__top(list);
	ASSERT_NULL(tmp0);
	list__destroy(list);
	list__destroy(list2);
}

CTEST(list, funny_thing)
{
	//Just to show that we can use this list with juste number without making a malloc
	List* list = list__create();
	list__add_front(list, (void*)3);
	list__add_front(list, (void*)2);
	list__add_front(list, (void*)1);
	int i = 0, *tmp;
	list__for_each(list, tmp) //tmp will be used as an int
	{
		if(i == 0)
			ASSERT_EQUAL(1, (long int)tmp);
		else if(i == 1)
			ASSERT_EQUAL(2, (long int)tmp);
		else if(i == 2)
			ASSERT_EQUAL(3, (long int)tmp);
		++i;
	}
	ASSERT_EQUAL(3, list__get_size(list));
	list__destroy(list);
}

