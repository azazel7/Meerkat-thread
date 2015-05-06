#pragma once

#define ALLOCATOR_THREAD 0
#define ALLOCATOR_STACK 1
void allocator_init(void);
void allocator_destroy(void);
void* allocator_malloc(int id_core, int type);
void allocator_free(int id_core, int type, void* data);
