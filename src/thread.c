#include <ucontext.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "list.h"
#include "htable.h"
#include "ordonnanceur.h"
#include "global.h"

#ifndef NDEBUG
#define FRPRINTF(chaine, __VA_ARGS__) fprintf(stderr, chaine, __VA_ARGS__)
#else
#define FRPRINTF(chaine, __VA_ARGS__) do{}while(0)
#endif

typedef int thread_t;

// La liste de tous les threads lancé.
List* all_thread = NULL;

// La liste de tous les threads fini qu'il faut libérer
List* all_thread_to_free = NULL;

// Le nombre de threads lancés. Identique à list__get_size(all_thread).
static int thread_count = 0;

// Un pointeur vers la structure du thread qui est en train de s'exécuter
thread_u *current_thread = NULL;

// Thread pour finir et nettoyer un thread fini
static thread_u ending_thread;

// Pour le timer, indique l'interval de temps
struct itimerval timeslice;

/*
 * Tableau associatif qui répertorie quel thread attend quel autre
 * La clef correspond a l'id du thread attendu et la donnée est la list (de type List*)
 * des threads (des thread_u, pas des id) qui l'attendent
 */
static htable* join_table = NULL;

// Tableau associatif qui, pour chaque thread fini (clef = id du thread), lui associe sa valeur de retour si elle existe (via un appel à thread_exit)
static htable* return_table = NULL;

// Variable qui servira à donner un id à chaque thread
static int global_id = 0;


// FIXME Les contexte partagent les buffer de printf !!!!!!!!! c'est normal

// L'appel à exit kill tout le monde ... ça aussi

// faire un return dans le main kill tout le monde

/*
 * Fonctions de l'API
 */

int thread_create(thread_t *newthread, void *(*start_routine)(void *), void *arg);

int thread_yield(void);

int thread_join(thread_t thread, void** retval);

thread_t thread_self(void);

void thread_exit(void *retval);

/*
 * Fonctions internes
 */

// Gestionnaire des signaux envoyé par l'horloge. Se contente ensuite d'appeler thread_schedul
void thread_handler(int sig);

// Fonction automatiquement appelé à la fin d'un thread qui se chargera de préparer le netoyage pour le scheduler et qui appelera le scheduler
void thread_end_thread(void);

// Replace tout les threads attendant le thread en argument dans la file d'exécution
void put_back_joining_thread_of(thread_u* thread);

// Si la fonction du thread fait un return, thread_catch_return récupérera la valeur et la mettra dans return_table pour que les thread qui join puissent l'avoir
void thread_catch_return(void * info);

// Initialise les variables globales et créer un thread pour le context actuel en plus
int thread_init(void);

// Libère la mémoire de tous les threads finis qui se trouvent dans la liste all_thread_to_free
void clear_finished_thread(void);

// Renvoie le nombre de cœurs de la machine
int get_number_of_core(void);

int thread_init(void) {
  fprintf(stderr, "First\n");

  // Ajoute les gestionnaires de signaux
  signal(SIGALRM, thread_handler);
  signal(SIGVTALRM, thread_handler);

  // Initialise les structures dans lesquelles on va ranger les données
  all_thread = list__create();
  join_table = htable__create_int();
  return_table = htable__create_int();
  all_thread_to_free = list__create();

  // Prépare le temps pour le timer
  timeslice.it_value.tv_sec = 2;
  timeslice.it_value.tv_usec = 0;

  // On re-init nous-même le timer pour ne pas compter le temps de re-scheduling
  timeslice.it_interval.tv_sec = 0;
  timeslice.it_interval.tv_usec = 0;

  // thread_count == 0 => slot == 0

  // Créer un contexte fait pour récupérer la fin d'un thread
  if(getcontext(&(ending_thread.ctx)) < 0)
    return -1;
  ending_thread.ctx.uc_stack.ss_sp = ending_thread.stack;
  ending_thread.ctx.uc_stack.ss_size = sizeof(ending_thread.stack);
  ending_thread.ctx.uc_link = NULL;

  // Information sur le thread pour l'api
  ending_thread.to_clean = false;
  ending_thread.is_joining = false;
  ending_thread.id = global_id++;

  // créer le contexte pour le contexte de nettoyage
  makecontext(&ending_thread.ctx, thread_end_thread, 1, NULL);

  // TODO find another way to create the thread from current "thread"

  // créer contexte avec getcontext pour le thread courant
  ++thread_count;

  current_thread = (thread_u *)malloc(sizeof(thread_u));

  if (current_thread == NULL) {

    thread_count = 0;
    return -1;

  }

  if (getcontext(&(current_thread->ctx)) < 0) {

    free(current_thread);
    thread_count = 0;
    return -1;

  }

  current_thread->to_clean = false;
  current_thread->is_joining = false;
  current_thread->id = global_id++;
  current_thread->ctx.uc_link = &ending_thread.ctx;
  current_thread->cr.function = NULL;
  current_thread->cr.arg = NULL;


  // Activer l'alarme et active l'intervalle

  if (setitimer(ITIMER_REAL, &timeslice, NULL) < 0)
    perror("setitimer");

  return 0;
}

int thread_create(thread_t *newthread, void *(*start_routine)(void *), void *arg) {

  // Alloue l'espace pour le thread
  thread_u* new_thread = (thread_u*)malloc(sizeof(thread_u));
  if (new_thread == NULL)
    return -1;

  // Créer le contexte pour le nouveau thread
  if (getcontext(&(new_thread->ctx)) < 0)
    return (free(new_thread), -1);

  // Définit le contexte ctx (où est la pile)
  new_thread->ctx.uc_stack.ss_sp = new_thread->stack;

  // Définit le contexte ctx (la taille de la pile)
  new_thread->ctx.uc_stack.ss_size = sizeof(new_thread->stack);

  // Quel contexte exécuter quand celui créé sera fini
  new_thread->ctx.uc_link = &ending_thread.ctx;

  // Infos supplémentaires sur le thread
  new_thread->to_clean = false;
  new_thread->is_joining = false;
  new_thread->id = global_id++;

  // Configure l'argument que l'on va donner à thread_catch_return qui exécutera start_routine
  new_thread->cr.function = start_routine;
  new_thread->cr.arg = arg;

  if(newthread != NULL)

    // Si l'utilisateur ne veut pas se souvenir du thread, on ne lui dit pas
    *newthread = new_thread->id;

  // créer le contexte
  makecontext(&(new_thread->ctx), (void (*)())thread_catch_return, 1, &new_thread->cr);

  if (thread_count == 0)
    if (thread_init() != 0)
      return (free(new_thread), -1);

  ++thread_count;

  // Échange le thread courant avec le nouveau thread
  thread_u *tmp = current_thread;
  current_thread = new_thread;
  new_thread = tmp;

  // new_thread correspond au thread courant

  // current_thread correspond au thread que l'on vient de créer et sur lequel on va switcher
  list__add_end(all_thread, new_thread);

  fprintf(stderr, "create then switch from %p to %p\n", new_thread, current_thread);

  // Sauvegarde le contexte actuel dans new_thread et switch sur le contexte de current_thread
  swapcontext(&(new_thread->ctx), &(current_thread->ctx));

  return 0;
}

void thread_end_thread() {
  fprintf(stderr, "Exit thread\n");

  // Informe the scheduler it will have to clean this thread
  current_thread->to_clean = true;

  // One thread less
  --thread_count;

  // If no more thread, we are the last
  if(thread_count <= 0) {
    clear_finished_thread();

    // Free everything
    list__destroy(all_thread);
    list__destroy(all_thread_to_free);

    // There is only one thread left, so join_table is empty
    htable__destroy(join_table);

    // TODO how to free all return values
    htable__destroy(return_table);
    free(current_thread);
    exit(0);
  }
  thread_schedul();
}

void thread_handler(int sig) {

  fprintf(stderr, "Alarm (%d)!\n", sig);
  thread_schedul();
}

int thread_yield(void) {

  if(thread_count != 0)
    thread_schedul();

  return 0;
}

int thread_join(thread_t thread, void** retval) {

  bool found = false;

  thread_u* th = NULL;

  if(current_thread == NULL || current_thread->id == thread)
    return;

  // Check if the thread exists in the runqueue

  // TODO also check among joining thread
  list__for_each(all_thread, th) {

    if(th->id == thread) {
	  found = true;
	  break;
    }
  }

  // If the thread isn't in the runqueu it is probably ended

  if(!found) {
    if(retval != NULL) {
      // If the thread is already finish, no need to wait and get its return value
      *retval = htable__find_int(return_table, thread);

      // If a return value, delete it like pthread do
      if(*retval != NULL)
	htable__remove_int(return_table, thread);
    }
    return 0;
  }

  // Find the list of all the threads joining thread
  List* join_on_thread = htable__find_int(join_table, thread);

  // If it does not exist, create it

  if (join_on_thread == NULL) {
    join_on_thread = list__create();
    htable__insert_int(join_table, thread, join_on_thread);
  }

  // Add the current_thread to the list of threads which are joining thread
  list__add_front(join_on_thread, current_thread);

  // Put information for the scheduler so it will know how to deal with it
  current_thread->is_joining = true;

  // switch of process
  thread_schedul();

  // We are back, just check the return value of thread and go back to work
  if(retval != NULL) {
    *retval =  htable__find_int(return_table, thread);

    // If we found the return value, delete it (like pthread)
    if(*retval != NULL)
     htable__remove_int(return_table, thread);
  }
  return 0;
}

thread_t thread_self(void) {

  if(thread_count != 0)
    return current_thread->id;

  return 2;


  /*Why 2 ? If there is no thread,
   * the created thread will be 0,
   * the ending thread 1,
   * and the current 2
   */
}

void thread_exit(void *retval) {

  // Insert retval into the return_table
  if(retval != NULL)
    htable__insert_int(return_table, current_thread->id, retval);

  // Finish exiting the thread by calling function to clean the thread
  thread_end_thread();
}

void put_back_joining_thread_of(thread_u* thread) {

  // Get the join_list to know all the threads joining thread
  List* join_on_thread = htable__find_int(join_table, thread->id);
  if(join_on_thread == NULL)
    return;

  // Put every joining thread back into the runqueue
  list__append(all_thread, join_on_thread);

  // Remove the entry from join_table
  htable__remove_int(join_table, thread);

  // Free the list
  list__destroy(join_on_thread);
}

void thread_catch_return(void * info) {
  catch_return* cr = (catch_return*)info;
  void* ret = cr->function(cr->arg);
  thread_exit(ret);
}

void clear_finished_thread(void) {
  void* tmp = NULL;
  list__for_each(all_thread_to_free, tmp) {
    list__remove_on_for_each(all_thread_to_free);
    free(tmp);
  }
}

int get_number_of_core(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

void thread_signal(int signal_to_set, void* (*function) (int) ) {
  current_thread->signals[signal_to_set] = function;
}
