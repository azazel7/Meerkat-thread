#include <stdbool.h>

typedef bool* our_mutex_t;

bool our_mutex__init(our_mutex_t* mutex);
bool our_mutex__lock(our_mutex_t mutex);
bool our_mutex__unlock(our_mutex_t mutex);
void our_mutex__destroy(our_mutex_t* mutex);
