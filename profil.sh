gcc src/*.c exemple/51-fibonacci.c -pg -o fibo `pkg-config --cflags glib-2.0`  -lpthread
./fibo 24
gprof fibo gmon.out | less
