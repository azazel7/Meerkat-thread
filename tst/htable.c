#include <stdlib.h>
#include <stdbool.h>
#include "ctest.h"
#include "../src/htable.h"

CTEST(htable, insert)
{
	htable* ht = htable__create_int();
	ASSERT_EQUAL(0, htable__size(ht));
	htable__insert(ht, 0, "duck");
	ASSERT_EQUAL(1, htable__size(ht));
	htable__destroy(ht);
}
CTEST(htable, insert1)
{
	htable* ht = htable__create_int();
	ASSERT_EQUAL(0, htable__size(ht));
	long int i;
	for(i = 0; i < 100; ++i)
		htable__insert(ht, (void*)i, "duck");
	ASSERT_EQUAL(100, htable__size(ht));
	htable__destroy(ht);
}
CTEST(htable, contain)
{
	htable* ht = htable__create_int();
	htable__insert(ht, 0, "duck");
	ASSERT_TRUE(htable__contain(ht, 0));
	ASSERT_FALSE(htable__contain(ht, (void*)1));
	htable__destroy(ht);
}
CTEST(htable, remove)
{
	htable* ht = htable__create_int();
	ASSERT_EQUAL(0, htable__size(ht));
	htable__insert(ht, 0, "duck");
	ASSERT_EQUAL(1, htable__size(ht));
	htable__remove(ht, (void*)1);
	ASSERT_EQUAL(1, htable__size(ht));
	htable__remove(ht, 0);
	ASSERT_EQUAL(0, htable__size(ht));
	htable__destroy(ht);
}
CTEST(htable, remove2)
{
	htable* ht = htable__create_int();
	ASSERT_EQUAL(0, htable__size(ht));
	htable__insert(ht, 0, "duck");
	htable__insert(ht, (void*)1, "duck");
	ASSERT_EQUAL(2, htable__size(ht));
	htable__remove(ht, 0);
	ASSERT_EQUAL(1, htable__size(ht));
	ASSERT_TRUE(htable__contain(ht, (void*)1));
	htable__destroy(ht);
}
CTEST(htable, remove3)
{
	htable* ht = htable__create_int();
	ASSERT_EQUAL(0, htable__size(ht));
	long int i;
	for(i = 0; i < 100; ++i)
		htable__insert(ht, (void*)i, "duck");
	ASSERT_EQUAL(100, htable__size(ht));
	htable__remove(ht, (void*)99);
	htable__remove(ht, (void*)50);
	ASSERT_EQUAL(98, htable__size(ht));
	for(i = 0; i < 100; ++i)
		if(i != 99 && i != 50)
			ASSERT_TRUE(htable__contain(ht, (void*)i));
	ASSERT_FALSE(htable__contain(ht, (void*)99));
	ASSERT_FALSE(htable__contain(ht, (void*)50));
	htable__destroy(ht);
}
CTEST(htable, find)
{
	htable* ht = htable__create_int();
	htable__insert(ht, 0, "duck");
	char* tmp = htable__find(ht, 0);
	ASSERT_STR("duck", tmp);
	htable__destroy(ht);
}
CTEST(htable, find2)
{
	htable* ht = htable__create_int();
	htable__insert(ht, 0, "duck");
	htable__insert(ht, 0, "otter");
	char* tmp = htable__find(ht, 0);
	ASSERT_STR("otter", tmp);
	htable__destroy(ht);
}
CTEST(htable, find3)
{
	htable* ht = htable__create_int();
	htable__insert(ht, 0, "duck");
	htable__insert(ht, (void*)1, "otter");
	char* tmp = htable__find(ht, 0);
	ASSERT_STR("duck", tmp);
	tmp = htable__find(ht, (void*)1);
	ASSERT_STR("otter", tmp);
	htable__remove(ht, 0);
	tmp = htable__find(ht, (void*)1);
	ASSERT_NOT_NULL(tmp);
	ASSERT_STR("otter", tmp);
	tmp = htable__find(ht, 0);
	ASSERT_NULL(tmp);
	htable__destroy(ht);
}
CTEST(htable, merge)
{
	htable* ht = htable__create_int();
	htable__insert(ht, 0, "duck");
	htable__insert(ht, (void*)1, "otter");
	htable* ht2 = htable__create_int();
	htable__insert(ht2, (void*)1, "canard");
	htable__insert(ht2, (void*)2, "loutre");
	htable__merge(ht, ht2);
	ASSERT_EQUAL(3, htable__size(ht));
	ASSERT_EQUAL(2, htable__size(ht2));
	ASSERT_TRUE(htable__contain(ht, 0));
	ASSERT_TRUE(htable__contain(ht, (void*)1));
	ASSERT_TRUE(htable__contain(ht, (void*)2));
	char* tmp = htable__find(ht, 0);
	ASSERT_STR("duck", tmp);
	tmp = htable__find(ht, (void*)1);
	ASSERT_STR("otter", tmp);
	tmp = htable__find(ht, (void*)2);
	ASSERT_STR("loutre", tmp);
	tmp = htable__find(ht2, (void*)1);
	ASSERT_STR("canard", tmp);
	tmp = htable__find(ht2, (void*)2);
	ASSERT_STR("loutre", tmp);
	htable__destroy(ht);
}
