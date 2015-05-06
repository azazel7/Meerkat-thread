#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <valgrind/valgrind.h>
#include <errno.h>
#include <sys/mman.h>
#include <glib.h>
#include <assert.h>
#include "global.h"
#include "meerkat.h"
#include "runqueue.h"
#include "mutex.h"
#include "allocator.h"

//La liste de tous les threads lancé.
LIST_HEAD(thread_u, runqueue);
mutex_t runqueue_mutex;
sem_t *semaphore_runqueue = NULL;

//Le nombre de thread lancé. Identique à list__get_size(runqueue).
static int thread_count = 0;

//Pour le timer, indique l'interval de temps (accédé uniquement par le pthread 0)
static struct itimerval timeslice;

//Variable qui servira à donner un id à chaque thread
static int global_id = 0;

core_information *core = NULL;
int number_of_core = 1;

//TODO add in core_information something to say it's already in thread change (for signal)

/*
 * Fonctions internes
 */

//Gestionnaire des signaux envoyé par l'horloge. Se contente ensuite d'appeler thread_schedul
void thread_handler(int sig);

//Le scheduler qui se charge de la tambouille pour changer et detruire les threads
void thread_schedul(volatile int);

//Replace tout les threads attendant le thread en argument dans la file d'exécution
void put_back_joining_thread_of(volatile thread_u * thread, int id_core);

//Si la fonction du thread fait un return, thread_catch_return récupérera la valeur et la mettra dans return_table pour que les thread qui join puissent l'avoir
void thread_catch_return(void *(*function) (void *), void *arg);

//Initialise les variables globales et créer un thread pour le context actuel en plus
int thread_init(void);

//Renvois l'id du cœur qui s'exécute
int get_idx_core(void);

//Gestionnaire pour ignorer un signal
void empty_handler(int s);

//Effectue le changement de context. Elle s'exécute sur une pile spéciale située dans chaque cœur.
void thread_change(int id_core);

//Initialise chaque cœur avec une petite différence d'initialisation pour le cœur 0.
void thread_init_i(int i, thread_u * current_thread);

void free_ressources(void);

void do_maintenance(int id_core);

__attribute__((constructor))
int thread_init(void)
{
	int i;
	FPRINTF("First\n");
	//Compte le nombre de cœurs disponibles
	number_of_core = 4;	//sysconf(_SC_NPROCESSORS_ONLN);

	//Ajoute les gestionnaire de signaux
	signal(SIGALRM, thread_handler);
	signal(SIGVTALRM, thread_handler);
	core = malloc(sizeof(core_information) * number_of_core);
	if(core == NULL)
		return -1;

	//Initialise le système d'allocation
	allocator_init();

	//Initialise les structures dans lesquelles on va ranger les donnée
	LIST_HEAD_INIT(runqueue);

	//Initialise les mutex
	mutex_init(&runqueue_mutex);

	//Créer le sémaphore
	semaphore_runqueue = sem_open("runqueue", O_CREAT, 0600, 0);

	//Si le sémaphore existait déjà, on l'initialise à 0
	sem_init(semaphore_runqueue, 0, 0);

	//Prépare le temps pour le timer
	timeslice.it_value.tv_sec = 2;
	timeslice.it_value.tv_usec = 0;

	//On re-init nous-même le timer pour ne pas compter le temps de re-schedul
	timeslice.it_interval.tv_sec = 0;
	timeslice.it_interval.tv_usec = 0;

	//Créer le thread courant
	++thread_count;
	thread_u *current_thread = (thread_u *) malloc(sizeof(thread_u));
	if(current_thread == NULL)
	{
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	
	//Initialise le thread courant
	current_thread->state = OTHER;
	current_thread->joiner = NULL;
	current_thread->return_value = NULL;
	current_thread->join_sync = 0;
	current_thread->id = global_id++;
	current_thread->ctx.uc_link = NULL;
	current_thread->stack = malloc(SIZE_STACK);
	LIST_INIT(runqueue, current_thread);
	//Enregistre la pile dans valgrind
	current_thread->valgrind_stackid = VALGRIND_STACK_REGISTER(current_thread->ctx.uc_stack.ss_sp, current_thread->ctx.uc_stack.ss_sp + SIZE_STACK);
	
	//Initialise les thread pthread (vue comme des cœurs par la suite pour des notions de sémantique)
	for(i = 0; i < number_of_core; ++i)
		thread_init_i(i, current_thread);

	//Activer l'alarm et active l'interval (si interval il y a)
	/*
	 * if(setitimer(ITIMER_REAL, &timeslice, NULL) < 0)
	 */
	/*
	 * perror("setitimer");
	 */
	return 0;
}

int thread_create(thread_t * newthread, void *(*start_routine) (void *), void *arg)
{
	//Alloue l'espace pour le thread
	thread_u *new_thread = (thread_u *) allocator_malloc(ALLOCATOR_THREAD);
	if(new_thread == NULL)
		return -1;

	//Créer le contexte pour le nouveau thread
	if(getcontext(&(new_thread->ctx)) < 0)
		return (free(new_thread), -1);

	//Définie le contexte ctx (où est la pile et sa taille)
	new_thread->stack = allocator_malloc(ALLOCATOR_STACK);
	new_thread->ctx.uc_stack.ss_sp = new_thread->stack;
	new_thread->ctx.uc_stack.ss_size = SIZE_STACK;

	//Quel contexte executer quand celui créé sera fini
	new_thread->ctx.uc_link = NULL;

	//Infos supplémentaire sur le thread
	new_thread->joiner = NULL;
	new_thread->state = OTHER;
	new_thread->return_value = NULL;
	new_thread->join_sync = 0;
	LIST_INIT(runqueue, new_thread);

	//Enregistre la pile
	new_thread->valgrind_stackid = VALGRIND_STACK_REGISTER(new_thread->ctx.uc_stack.ss_sp, new_thread->ctx.uc_stack.ss_sp + SIZE_STACK);

	//créer le contexte
	makecontext(&(new_thread->ctx), (void (*)())thread_catch_return, 2, start_routine, arg);

	//On met l'id avec global_id après un eventuel appel à thread_init. (Ce qui garanti d'avoir toujours la même répartion des id
	new_thread->id = __sync_fetch_and_add(&global_id, 1);

	//Attend d'avoir initialiser l'id avant de le donner
	if(newthread != NULL)
		//Si l'utilisateur ne veut pas se souvenir du thread, on ne lui dit pas
		*newthread = new_thread;

	//Ajoute un thread au total
	__sync_add_and_fetch(&thread_count, 1);

	//new_thread correspond au thread courant
	add_begin_thread_to_runqueue(get_idx_core(), new_thread, MIDDLE_PRIORITY);
	FPRINTF("Create new thread %d (%p) stack (%p) on core %d\n", new_thread->id, new_thread, new_thread->stack, get_idx_core());
	return 0;
}

//It's volatile to avoid optimization (id_core is compute again later)
void thread_schedul(volatile int id_core)
{
	IGNORE_SIGNAL(SIGALRM);

	//previous can be NULL when each pthread thread call thread_schedul for the first time
	CURRENT_CORE.previous = CURRENT_THREAD;
	FPRINTF("Pause %d on core %d\n", CURRENT_CORE.previous->id, id_core);

	//If the current thread is not finish, we push him back into the runqueu
	if(CURRENT_CORE.previous != NULL && CURRENT_CORE.previous->state == FINISHED)
		put_back_joining_thread_of(CURRENT_CORE.previous, id_core);

	//Try to get a thread from the runqueue (return NULL else)
	CURRENT_THREAD = try_get_thread_from_runqueue(id_core);

	//If there is no thread to execute, switch to an other stack (safe stack) to clean everything
	if(CURRENT_THREAD == NULL)
		if(CURRENT_CORE.previous->state != FINISHED)
			swapcontext(&(CURRENT_CORE.previous->ctx), &(CURRENT_CORE.ctx));
		else
			setcontext(&(CURRENT_CORE.ctx));
	//Else, switch to the thread
	else
		if(CURRENT_CORE.previous->state != FINISHED)
			swapcontext(&(CURRENT_CORE.previous->ctx), &(CURRENT_THREAD->ctx));
		else
			setcontext(&(CURRENT_THREAD->ctx));

	//Compute again id_core because we probably change the core since last time
	id_core = get_idx_core();
	//Call maintenance if needed. (To clean previous thread, to put in joining mode, ...)
	if(CURRENT_CORE.previous != NULL)
		do_maintenance(id_core);

	FPRINTF("Start %d on core %d\n", CURRENT_THREAD->id, id_core);
	UNIGNORE_SIGNAL(SIGALRM);
}

void thread_handler(int sig)
{
	int i;
	int id_core = get_idx_core();
	printf("Alarm\n");

	//TODO ré-armer le signal (mais pas sûr)
	//If we are the first core, we are the only one to receive this signal so warn the others
	if(id_core == 0)
		for(i = 1; i < number_of_core; ++i)
			pthread_kill(core[i].thread, SIGALRM);

	//If the current core doesn't execute something, it's sleeping, so it already is in thread_schedul
	//TODO change that by an attribut
	if(CURRENT_THREAD != NULL)
		thread_schedul(id_core);
}

int thread_yield(void)
{
	//XXX probably change that by a macro ...
	thread_schedul(get_idx_core());
}

int thread_join(volatile thread_t thread, void **retval)
{
	int id_core = get_idx_core();
	//If trying to join itself, stop it
	if(CURRENT_THREAD == thread)
		return -1;
	FPRINTF("%d try to join %d on core %d\n", CURRENT_THREAD->id, ((thread_u*)thread)->id, id_core);

	//Try to put our value into join_sync if no value yet
	char old = __sync_val_compare_and_swap(&((thread_u*)thread)->join_sync, 0, 1);
	if(old == 2)
	{
		//If join_sync was 2, it mean the thread we are trying to join is dying. so we wait till is became a zombi
		FPRINTF("%d wait for %d on core %d\n", CURRENT_THREAD->id, ((thread_u*)thread)->id, id_core);
		while(((thread_u*)thread)->state != ZOMBI);
	}
	else
	{
		//If it's our value in join_sync, it's mean we are the first to synchronize so we start to join and switch to an other thread
		FPRINTF("%d rescheduled on core %d\n", CURRENT_THREAD->id, id_core);
		((thread_u*)thread)->joiner = CURRENT_THREAD;
		CURRENT_THREAD->state = GOING_TO_JOIN;

		//switch of process
		thread_schedul(id_core);
	}

	//We are back, just check the return value of thread and go back to work
	if(retval != NULL)
		*retval = ((thread_u*)thread)->return_value;
	//From now the thread we are joining is dead and its stack has been freed so we free the structure
	allocator_free(ALLOCATOR_THREAD, thread);
	return 0;
}

thread_t thread_self(void)
{
	int id_core = get_idx_core();
	return CURRENT_THREAD;

	//Why 1 ? If there is no thread
	//The created thread will be 2
	//The ending thread 0
	//And the current 1
}

void thread_exit(void *retval)
{
	int id_core = get_idx_core();

	//Insert retval into the return_table
	CURRENT_THREAD->return_value = retval;
	
	//Finish exiting the thread by calling function to clean the thread
	FPRINTF("Exit thread %d on core %d\n", CURRENT_THREAD->id, id_core);

	//Informe the scheduler he will have to clean this thread
	CURRENT_THREAD->state = FINISHED;

	//One thread less
	bool is_no_more_thread = __sync_sub_and_fetch(&thread_count, 1) <= 0;

	//If no more thread, we are the last
	if(is_no_more_thread)
	{
		FPRINTF("Finishing by %d on core %d\n", CURRENT_THREAD->id, id_core);

		// call freeRessources to free global variable
		exit(0);
	}
	//Or switch to an other thread
	thread_schedul(id_core);
}

void put_back_joining_thread_of(volatile thread_u * thread, int id_core)
{
	FPRINTF("Put back joiner of %d on core %d\n", thread->id, get_idx_core());
	//Try to put 2 into join_sync
	char old = __sync_val_compare_and_swap(&thread->join_sync, 0, 2);
	//if join_sync equal 1 instead, that mean a joiner is coming
	if(old == 1)
	{
		FPRINTF("Wait for joiner of %d on core %d\n", thread->id, get_idx_core());
		//Wait till the joiner put himself into thread->joiner
		while(thread->joiner == NULL);		
		//Wait till the joiner beging JOINING so it will be safe to execute it again
		while(thread->joiner->state != JOINING);		
		//Change the state
		thread->joiner->state = OTHER;
		//Put it at the beginning of the runqueue and let's die in peace
		add_begin_thread_to_runqueue(id_core, (thread_u*)thread->joiner, HIGH_PRIORITY);
	}
	//Else, the joiner came first so it's waiting for us. Don't put it into the runqueue and die as quick as you can
}

void thread_catch_return(void *(*function) (void *), void *arg)
{
	int id_core = get_idx_core();
	//Do a bit of maintenance
	if(CURRENT_CORE.previous != NULL)
		do_maintenance(id_core);
	
	thread_exit(function(arg));
}

int get_idx_core(void)
{
	int i;
	pthread_t self = pthread_self();
	for(i = 0; i < number_of_core; ++i)
		if(self == core[i].thread)
			return i;
	return -1;
}

void empty_handler(int s)
{
	printf("Call in empty handler core %d\n", get_idx_core());
}

void thread_change(int id_core)
{
	//This function is always called on a the safe stack
	//Do a little maintenance
	if(CURRENT_CORE.previous != NULL)
		do_maintenance(id_core);
	
	CURRENT_THREAD = NULL;
	CURRENT_CORE.previous = NULL;

	//Get the next thread from the runqueu
	CURRENT_THREAD = get_thread_from_runqueue(id_core);

	//Then switch to the thread context. No need to use swapcontext because the current context is not useful anymore
	if(CURRENT_THREAD != NULL)
	{
		FPRINTF("Start %d on core %d\n", CURRENT_THREAD->id, id_core);
		setcontext(&(CURRENT_THREAD->ctx));
	}
	printf("Error CURRENT_THREAD is NULL (core %d)\n", id_core);
	exit(0);
}

void thread_init_i(int i, thread_u * current_thread)
{

	//Initialise les variable du cœur
	core[i].previous = NULL;
	LIST_HEAD_INIT(core[i].runqueue);
	core[i].stack_to_free = NULL;
	if(getcontext(&(core[i].ctx)) < 0)
		exit(-1);

	//Définie le contexte ctx (où est la pile)
	core[i].ctx.uc_stack.ss_sp = core[i].stack;

	//Définie le contexte ctx (la taille de la pile)
	core[i].ctx.uc_stack.ss_size = sizeof(core[i].stack);

	//Quel contexte executer quand celui créé sera fini
	core[i].ctx.uc_link = NULL;

	//Configure le contexte à exécuter quand le cœur fera de l'ordonnancement
	makecontext(&(core[i].ctx), (void (*)())thread_change, 1, i);

	//Enregistre la pile du contexte du cœur comme cela valgrind sera sympa avec nous
	core[i].valgrind_stackid = VALGRIND_STACK_REGISTER(core[i].ctx.uc_stack.ss_sp, core[i].ctx.uc_stack.ss_sp + SIZE_STACK);
	if(i == 0)					//Le cœur 0 à un traitement spécial car c'est lui qui exécute le code
	{
		core[0].thread = pthread_self();
		core[0].current = current_thread;
	}
	else
	{
		core[i].current = NULL;

		//Lancement du thread pthread (soit un cœur)
		pthread_create(&(core[i].thread), NULL, (void *(*)(void *))thread_change, (void *)(long int)i);
	}
}

// La fonction free_resources est appellée à la fin du programme, et libère les ressources globales.
__attribute((destructor))
static void ending_process()
{
	if(core == NULL)
		return;
	int i;
	int id_core = get_idx_core();
	thread_u* tmp = CURRENT_THREAD;
	CURRENT_THREAD = NULL;
	for(i = 0; i < number_of_core; ++i)
	{
		while(core[i].previous != NULL && core[i].current != NULL);
	}
	sem_close(semaphore_runqueue);
	sem_destroy(semaphore_runqueue);
	mutex_destroy(&runqueue_mutex);
	free(tmp->stack);
	free(tmp);
	free(core);
}

void free_ressources(void)
{
	int i;
	for(i = 0; i < number_of_core; ++i)
		VALGRIND_STACK_DEREGISTER(core[i].valgrind_stackid);
	FPRINTF("Finished by core %d\n", get_idx_core());
}

void do_maintenance(int id_core)
{
	if(CURRENT_CORE.previous->state == FINISHED)
	{
		FPRINTF("Free stack of %d on core %d\n", CURRENT_CORE.previous->id, id_core);
		VALGRIND_STACK_DEREGISTER(CURRENT_CORE.previous->valgrind_stackid);
		//Free the stack
		allocator_free(ALLOCATOR_STACK, CURRENT_CORE.previous->stack);
		//And "wait" for a joiner
		CURRENT_CORE.previous->state = ZOMBI;
	}
	else if(CURRENT_CORE.previous->state == GOING_TO_JOIN)	//FIXME don't know why, but if you use id_joining, it doesn't work.
	{
		CURRENT_CORE.previous->state = JOINING;
	}
	else
	{
		add_thread_to_runqueue(id_core, CURRENT_CORE.previous);
	}
}
