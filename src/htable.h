#pragma once

#include <stdbool.h>
#define SIZE_HASH_TABLE 30

struct list;
typedef struct list List;
struct htable;
typedef struct htable htable;

/**
 * brief Creates a new hash table
 * param hashe a function which has to return the hash of the data between 0 and SIZE_HASH_TABLE
 * param cmp a function which has to return 1 if both of its parameters are equal and 0 else 
 */
htable *htable__create(int (*hash) (), bool(*cmp) (), bool use_lock);
htable* htable__create_int(bool use_lock);

/**

 * @brief Checks if an element exists in the hash table.
 * @return 1 if the element exists
 */
bool htable__contain(htable* h_table, void*);
#define htable__contain_int(h_table, key) htable__contain(h_table, (void*)(long int)key)

/**
 * @brief Adds an element in the hash table.
 * If element exists, it will be inserted two times.
 */
void htable__insert(htable *h_table, void* key, void *data);
#define htable__insert_int(h_table, key, data) htable__insert(h_table, (void*)(long int)key, data)

/**
 * @brief Removes an element from the hash table.
 * The elements have to exist in the hash table.
 */
void* htable__remove(htable* h_table, void*);
#define htable__remove_int(h_table, key) htable__remove(h_table, (void*)(long int)key)

/**
 * @brief Gets the number of elements in hash table.
 * Returns the number of elements.
 */
int htable__size(htable* h_table);

/**
 * fn htable__get_element
 * brief Gets an element from the hash table.
 * return An element from the hash table or NULL
 */
bool htable__get_element(htable* h_table, void ** key, void** data);

/**
 
  @brief Finds the data in the hash table.
 */
void* htable__find(htable* h_table, void* data); 
#define htable__find_int(h_table, key) htable__find(h_table, (void*)(long int)key) 

void* htable__find_and_apply(htable* h_table, void* data, void(*)(void*)); 

/**
 
 @brief Applies a function on each element of an h_table.
 */
void htable__apply(htable* h_table, void (*function)(void*));

/**

 @brief Frees the memory of an hash table.
 */
void htable__destroy(htable* h_table);


