#pragma once

#define HIGH_PRIORITY 0
#define MIDDLE_PRIORITY 1
struct thread_u;
typedef struct  thread_u thread_u;

thread_u *get_thread_from_runqueue(int id_core);
thread_u* try_get_thread_from_runqueue(int id_core);
void add_thread_to_runqueue(int id_core, thread_u * thread);
void add_begin_thread_to_runqueue(int id_core, thread_u * thread, int priority);
