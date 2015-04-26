#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include "meerkat.h"

/* test du join, avec ou sans thread_exit.
 *
 * le programme doit retourner correctement.
 * valgrind doit être content.
 *
 * support nécessaire:
 * - thread_create()
 * - thread_exit()
 * - retour sans thread_exit()
 * - thread_join() avec récupération valeur de retour, avec et sans thread_exit()
 */

static void * thfunc(void *dummy __attribute__((unused)))
{
  thread_exit((void*)0xdeadbeef);
}

static void * thfunc2(void *dummy __attribute__((unused)))
{
  return (void*) 0xbeefdead;
}

int main()
{
  thread_t th, th2;
  int err;
  void *res = NULL;
  struct timeval tv1, tv2;
  unsigned long us;
  gettimeofday(&tv1, NULL);
  err = thread_create(&th, thfunc, NULL);
  assert(!err);
  err = thread_create(&th2, thfunc2, NULL);
  assert(!err);

  err = thread_join(th, &res);
  assert(!err);
  assert(res == (void*) 0xdeadbeef);

  err = thread_join(th2, &res);
  assert(!err);
  assert(res == (void*) 0xbeefdead);
  gettimeofday(&tv2, NULL);
  us = (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
  printf("time : %ld us\n", us);

  printf("join OK\n");
  return 0;
}
