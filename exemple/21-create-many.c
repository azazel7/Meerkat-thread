#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "meerkat.h"

/* test de plein de create-destroy consécutifs.
 *
 * valgrind doit etre content.
 * la durée du programme doit etre proportionnelle au nombre de threads donnés en argument.
 * jusqu'à combien de threads cela fonctionne-t-il ?
 *
 * support nécessaire:
 * - thread_create()
 * - thread_exit()
 * - thread_join() avec récupération de la valeur de retour
 */

static void * thfunc(void *dummy __attribute__((unused)))
{
  thread_exit(NULL);
}

int main(int argc, char *argv[])
{
  thread_t th;
  int err, i, nb;
  void *res;

	(void)err;
  if (argc < 2) {
    printf("argument manquant: nombre de threads\n");
    return -1;
  }

  nb = atoi(argv[1]);
  struct timeval tv1, tv2;
  unsigned long us;
  gettimeofday(&tv1, NULL);
  
  for(i=0; i<nb; i++) {
    err = thread_create(&th, thfunc, NULL);
    assert(!err);
    err = thread_join(th, &res);
    assert(!err);
    assert(res == NULL);
  }
  gettimeofday(&tv2, NULL);
  us = (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
  printf("time : %ld us\n", us);

  printf("%d threads créés et détruits\n", nb);
  return 0;
}
