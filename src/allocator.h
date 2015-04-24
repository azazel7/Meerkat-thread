#pragma once

#define ALLOCATOR_THREAD 0
#define ALLOCATOR_LIST_NODE 2
#define ALLOCATOR_HTABLE_NODE 1
void allocator_init(void);
void allocator_destroy(void);
void* allocator_malloc(int type);
void allocator_free(int type, void* data);
