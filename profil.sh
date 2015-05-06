gcc src/*.c exemple/51-fibonacci.c -pg -o fibo `pkg-config --cflags glib-2.0`  -lpthread
./fibo 33
gprof -Pmutex_init -Pget_thread_from_runqueue -Pthread_init -Pallocator_init -Pthread_init_i -Qmutex_init -Qget_thread_from_runqueue -Qthread_init -Qallocator_init -Qthread_init_i fibo gmon.out | less
