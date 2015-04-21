#pragma once

struct thread_u;
typedef struct  thread_u thread_u;

thread_u * get_thread_from_runqueue(void);
void add_thread_to_runqueue(thread_u* thread);
