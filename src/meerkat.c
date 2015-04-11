#include <ucontext.h>
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
#include "list.h"
#include "htable.h"

//The size of the stack of each thread. Work well with 16384. Error if less or equal than 8192
#define SIZE_STACK 16384
#define CURRENT_CORE core[id_core]
#define CURRENT_THREAD core[id_core].current
#define IGNORE_SIGNAL(i) signal(i, empty_handler)
#define UNIGNORE_SIGNAL(i) signal(i, thread_handler)

typedef struct catch_return
{
	void* (*function)(void*);
	void* arg;
} catch_return;
typedef struct thread_u
{
	int id;
	
  //Context for swapcontext, setcontext, ... and everything
	ucontext_t ctx;
	
  //Stack for the thread
	char stack[SIZE_STACK];
	
  //Information so the scheduler will know if he must remove the thread
	bool to_clean;
	
  //Information for the scheduler
	bool is_joining;
	int valgrind_stackid;
	catch_return cr;
	int id_joining;
} thread_u;
typedef struct core_information
{
	pthread_t thread;
	thread_u* current;
	thread_u* previous;
	ucontext_t ctx;
	char stack[SIZE_STACK];
	bool unlock_runqueue;
	bool unlock_join_queue;
	int valgrind_stackid;
} core_information;

typedef int thread_t;

//La liste de tous les threads lancé.
static List* runqueue = NULL;
static pthread_mutex_t runqueue_mutex;
static sem_t* semaphore_runqueue;

  //Le nombre de thread lancé. Identique à list__get_size(runqueue).
static int thread_count = 0;
static pthread_mutex_t thread_count_mutex;

  //Thread pour finir et nettoyer un thread fini
static thread_u ending_thread;

  //Pour le timer, indique l'interval de temps (accédé uniquement par le pthread 0)
static struct itimerval timeslice;

  //Tableau associatif qui répertorie quel thread attend quel autre
//La clef correspond a l'id du thread attendu et la donnée est la list (de type List*) des threads (des thread_u, pas des id) qui l'attendent
static List* join_queue = NULL;
static pthread_mutex_t join_queue_mutex;

  //Tableau associatif qui, pour chaque thread fini (clef = id du thread), lui associe sa valeur de retour si elle exist (via un appel à thread_exit)
static htable* return_table = NULL;
static pthread_mutex_t return_table_mutex;

  //Variable qui servira à donner un id à chaque thread
static int global_id = 0;
static pthread_mutex_t global_id_mutex;
//
static core_information* core = NULL;

  //TODO ajouter des verrous pour toutes les ressources partagées
//FIXME Les contexte partagent les buffer de printf !!!!!!!!!
//L'appel à exit kill tout le monde ...
//faire un return dans le main kill tout le monde

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

  //Gestionnaire des signaux envoyé par l'horloge. Se contente ensuite d'appeler thread_schedul
void thread_handler(int sig);

  //Fonction automatiquement appelé à la fin d'un thread qui se chargera de préparer le netoyage pour le scheduler et qui appelera le scheduler
void thread_end_thread(void);

  //Le scheduler qui se charge de la tambouille pour changer et detruire les threads
void thread_schedul(void);

  //Replace tout les threads attendant le thread en argument dans la file d'exécution
void put_back_joining_thread_of(thread_u* thread);

  //Si la fonction du thread fait un return, thread_catch_return récupérera la valeur et la mettra dans return_table pour que les thread qui join puissent l'avoir
void thread_catch_return(void * info);

  //Initialise les variables globales et créer un thread pour le context actuel en plus
int thread_init(void);

  //Renvois le nombre de cœurs de la machine
int get_number_of_core(void);

  //Renvois l'id du cœur qui s'exécute
int get_idx_core(void);

  //Gestionnaire pour ignorer un signal
void empty_handler(int s);

  //Cette ne doit être appelé que par un unique core (pthread) à la fois
void print_core_info(void);

  //Effectue le changement de context. Elle s'exécute sur une pile spéciale située dans chaque cœur.
void thread_change(int id_core);

  //Initialise chaque cœur avec une petite différence d'initialisation pour le cœur 0.
void thread_init_i(int i, thread_u* current_thread);

int thread_init(void)
{
		int i;
		fprintf(stderr, "First\n");
		
  //Ajoute les gestionnaire de signaux
		signal(SIGALRM, thread_handler);
		signal(SIGVTALRM, thread_handler);
		printf("DD %d\n", sizeof(core_information));
		core  = malloc(sizeof(core_information)*get_number_of_core());
		if(core == NULL)
			return -1;
		
  //Initialise les structures dans lesquelles on va ranger les donnée
		runqueue = list__create();
		join_queue = list__create();
		return_table = htable__create_int();
		
  //Initialise les mutex
		pthread_mutex_init(&global_id_mutex, NULL);
		pthread_mutex_init(&thread_count_mutex, NULL);
		pthread_mutex_init(&runqueue_mutex, NULL);
		pthread_mutex_init(&join_queue_mutex, NULL);
		pthread_mutex_init(&return_table_mutex, NULL);
		
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
		
  //thread_count == 0 => slot == 0
		ending_thread.ctx.uc_stack.ss_sp = ending_thread.stack;
		ending_thread.ctx.uc_stack.ss_size = sizeof(ending_thread.stack);
		ending_thread.ctx.uc_link = NULL;
		
  //Information sur le thread pour l'api
		ending_thread.to_clean = false;
		ending_thread.is_joining = false;
		
  //Un seul thread pour le moment, donc pas besoin de verrou
		ending_thread.id = global_id++;
		
  //créer le contexte pour le contexte de netoyage
		makecontext(&ending_thread.ctx, thread_end_thread, 1, NULL);
		++thread_count;
		thread_u* current_thread = (thread_u *)malloc(sizeof(thread_u));
		if(current_thread == NULL)
		{
			thread_count = 0;
			return -1;
		}
		current_thread->to_clean = false;
		current_thread->is_joining = false;
		current_thread->id_joining = -1;
		
  //Un seul thread pour le moment, donc pas besoin de verrou
		current_thread->id = global_id++;
		current_thread->ctx.uc_link = &ending_thread.ctx;
		current_thread->cr.function = NULL;
		current_thread->cr.arg = NULL;
		
  //Enregistre la pile dans valgrind
		current_thread->valgrind_stackid = VALGRIND_STACK_REGISTER(current_thread->ctx.uc_stack.ss_sp, current_thread->ctx.uc_stack.ss_sp + SIZE_STACK);
		
  //Initialise les thread pthread (vue comme des cœurs par la suite pour des notions de sémantique)
		for(i = 0; i < get_number_of_core(); ++i)
			thread_init_i(i, current_thread);
		
  //Activer l'alarm et active l'interval (si interval il y a)
		/*if(setitimer(ITIMER_REAL, &timeslice, NULL) < 0)*/
			/*perror("setitimer");*/
		return 0;
}

int thread_create(thread_t *newthread, void *(*start_routine)(void *), void *arg)
{
	//Alloue l'espace pour le thread
	thread_u* new_thread = (thread_u*)malloc(sizeof(thread_u));
	if(new_thread == NULL)
		return -1;
	
  //Créer le contexte pour le nouveau thread
	if(getcontext(&(new_thread->ctx)) < 0)
		return (free(new_thread), -1);
	
  //Définie le contexte ctx (où est la pile)
	new_thread->ctx.uc_stack.ss_sp = new_thread->stack;
	
  //Définie le contexte ctx (la taille de la pile)
	new_thread->ctx.uc_stack.ss_size = sizeof(new_thread->stack);
	
  //Quel contexte executer quand celui créé sera fini
	new_thread->ctx.uc_link = &ending_thread.ctx;
	
  //Infos supplémentaire sur le thread
	new_thread->to_clean = false;
	new_thread->is_joining = false;
	new_thread->id_joining = -1;
	
  //Configure l'argument que l'on va donner à thread_catch_return qui exécutera start_routine
	new_thread->cr.function = start_routine;
	new_thread->cr.arg = arg;
	
  //Enregistre la pile
	new_thread->valgrind_stackid = VALGRIND_STACK_REGISTER(new_thread->ctx.uc_stack.ss_sp, new_thread->ctx.uc_stack.ss_sp + SIZE_STACK);

  //créer le contexte
	makecontext(&(new_thread->ctx), (void (*)())thread_catch_return, 1, &new_thread->cr);
	if(thread_count == 0)
		if(thread_init() != 0)
		{
			VALGRIND_STACK_DEREGISTER(new_thread->valgrind_stackid);
			free(new_thread);
			return -1;
		}
	
  //On met l'id avec global_id après un eventuel appel à thread_init. (Ce qui garanti d'avoir toujours la même répartion des id
	pthread_mutex_lock(&global_id_mutex);
	new_thread->id = global_id++;
	pthread_mutex_unlock(&global_id_mutex);
	
  //Attend d'avoir initialiser l'id avant de le donner
	if(newthread != NULL) 
  //Si l'utilisateur ne veut pas se souvenir du thread, on ne lui dit pas
		*newthread = new_thread->id;

	int id_core = get_idx_core();
	pthread_mutex_lock(&thread_count_mutex);
	++thread_count;
	pthread_mutex_unlock(&thread_count_mutex);
	
  //new_thread correspond au thread courant
	pthread_mutex_lock(&runqueue_mutex);
	list__add_end(runqueue, new_thread);
	sem_post(semaphore_runqueue);
	fprintf(stderr, "Create new thread %d on core %d\n", new_thread->id, id_core);
	pthread_mutex_unlock(&runqueue_mutex);
	return 0;
}

void thread_end_thread()
{
	int id_core = get_idx_core();
	fprintf(stderr, "Exit thread %d on core %d\n", CURRENT_THREAD->id, id_core);
	
  //Informe the scheduler he will have to clean this thread
	CURRENT_THREAD->to_clean = true;
	
  //One thread less
	pthread_mutex_lock(&thread_count_mutex);
	--thread_count;
	bool is_no_more_thread = thread_count <= 0;
	pthread_mutex_unlock(&thread_count_mutex);
	
  //If no more thread, we are the last
	if(is_no_more_thread)//TODO how to finish this stuff
	{
		fprintf(stderr, "Finishing by %d on core %d\n", CURRENT_THREAD->id, id_core);
		thread_u* current_thread = CURRENT_THREAD;
		int i;
		for(i = 0; i < get_number_of_core(); ++i)
		{
			if(i != id_core)
				pthread_kill(core[i].thread, SIGKILL);
			VALGRIND_STACK_DEREGISTER(core[i].valgrind_stackid);
		}
		free(core);
		
  //Free every thing
		list__destroy(runqueue);
		pthread_mutex_destroy(&global_id_mutex);
		pthread_mutex_destroy(&thread_count_mutex);
		pthread_mutex_destroy(&runqueue_mutex);
		pthread_mutex_destroy(&join_queue_mutex);
		pthread_mutex_destroy(&return_table_mutex);
		sem_close(semaphore_runqueue);
		sem_destroy(semaphore_runqueue);
		
  //There is only one thread left, so join_table is empty
		list__destroy(join_queue);
		
  //TODO how to free all return values
		htable__remove_int(return_table, current_thread->id);
		htable__destroy(return_table);
		fprintf(stderr, "Finished by core %d\n", id_core);
		free(current_thread);
		exit(0);
	}
	thread_schedul();
}

void thread_schedul()
{
	IGNORE_SIGNAL(SIGALRM);
	int id_core = get_idx_core();
	bool current_thread_dead = false;
	
  //previous can be NULL when each pthread thread call thread_schedul for the first time 
	CURRENT_CORE.previous = CURRENT_THREAD;
	fprintf(stderr, "Pause %d on core %d\n", CURRENT_CORE.previous->id, id_core);
	
  //If the current thread is not finish, we push him back into the runqueu
	if(CURRENT_CORE.previous != NULL && CURRENT_CORE.previous->to_clean)
	{
		
  //So the current thread is no more active which mean less deadlock with the join
		//The join will hopefully no see it
		//the return value is already in the return table
		CURRENT_THREAD = NULL;
		
  //Bring back in the runqueu threads that have joined the current_thread
		put_back_joining_thread_of(CURRENT_CORE.previous);
		
  //Do not free here, cause you will free the stack on which you are executed.
		//It's like cut the branch you sit on. It's stupid.
		fprintf(stderr, "Plan deletion %d on core %d\n", CURRENT_CORE.previous->id, id_core);
		
  //There is no more CURRENT_CORE.previous because we free it
		current_thread_dead = true;
	}
	else if(CURRENT_CORE.previous != NULL && !CURRENT_CORE.previous->is_joining)//FIXME don't know why, but if you use id_joining, it doesn't work.
	{
		pthread_mutex_lock(&runqueue_mutex);
		list__add_end(runqueue, CURRENT_CORE.previous);
		sem_post(semaphore_runqueue);
		
  //Plan to release the lock once we are finished with the current stack
		CURRENT_CORE.unlock_runqueue = true;
	}
	
  //Switch to the core context so we will be on a new stack
	if(!current_thread_dead)
		swapcontext(&(CURRENT_CORE.previous->ctx), &(CURRENT_CORE.ctx));
	else
		setcontext(&(CURRENT_CORE.ctx));
	UNIGNORE_SIGNAL(SIGALRM);
}

void thread_handler(int sig)
{
	int i;
	int id_core = get_idx_core();
	printf("Alarm\n");
	if(id_core == 0)
		print_core_info();
	
  //TODO ré-armer le signal (mais pas sûr)
	//If we are the first core, we are the only one to receive this signal so warn the others
	if(id_core == 0)
		for(i = 1; i < get_number_of_core(); ++i)
			pthread_kill(core[i].thread, SIGALRM);
	
  //If the current core doesn't execute something, it's sleeping, so it already is in thread_schedul
	if(CURRENT_THREAD != NULL)
		thread_schedul();
}

int thread_yield(void)
{
	if(thread_count != 0)
	{
		if(get_idx_core() == 0)
			print_core_info();
		thread_schedul();
	}
	return 0;
}

int thread_join(thread_t thread, void** retval)
{
	bool found = false;
	int i;
	thread_u* th = NULL;
	int id_core = get_idx_core();
	if(CURRENT_THREAD == NULL || CURRENT_THREAD->id == thread)
		return;
	fprintf(stderr, "%d try to join %d on core %d\n", CURRENT_THREAD->id, thread, id_core);
	
  //Check if the thread exist in the runqueu
	//Put also join_queue_mutex because if put_back_joining_thread_of is called after we've checked it could be a problem
	pthread_mutex_lock(&join_queue_mutex);
	pthread_mutex_lock(&runqueue_mutex);
	
  //Check on current working thread
	for( i = 0;found == false && i < get_number_of_core(); ++i)
	{
		if(i != id_core && core[i].current != NULL && core[i].current->id == thread)
			found = true;
	}
	
  //Check on the runqueue
	if(!found)
	{
		list__for_each(runqueue, th)
		{
			if(th->id == thread)
			{
				found = true;
				break;
			}
		}
	}
	
  //Do not move up the unlock, because other are less likely to change their current
	pthread_mutex_unlock(&runqueue_mutex);
	
  //check on the join_queue
	if(!found)
	{
		list__for_each(join_queue, th)
		{
			if(th->id == thread)
			{
				found = true;
				break;
			}
		}
	}
	
  //If the thread isn't in the runqueu it is probably ended
	if(!found)
	{
		pthread_mutex_unlock(&join_queue_mutex);
		if(retval != NULL)//If the thread is already finish, no need to wait and get its return value
		{
			pthread_mutex_lock(&return_table_mutex);
			*retval =  htable__find_int(return_table, thread);
			
  //If a return value, delete it like pthread do
			if(*retval != NULL)
				htable__remove_int(return_table, thread);
			pthread_mutex_unlock(&return_table_mutex);
		}
		return 0;
	}
	
  //Find the list of all the threads joining thread
	CURRENT_THREAD->id_joining = thread;
	CURRENT_THREAD->is_joining = true;
	list__add_front(join_queue, CURRENT_THREAD);
	
  //Put information for the scheduler so he will know how to deal with it
	fprintf(stderr, "%d join %d on core %d\n", CURRENT_THREAD->id, thread, id_core);
	
  //Plan to release the lock once we are finished with the current stack
	CURRENT_CORE.unlock_join_queue = true;
	
  //switch of process
	thread_schedul();
	
  //We are back, just check the return value of thread and go back to work
	if(retval != NULL)
	{
		pthread_mutex_lock(&return_table_mutex);
		*retval =  htable__find_int(return_table, thread);
		
  //If we found the return value, delete it (like pthread)
		if(*retval != NULL)
			htable__remove_int(return_table, thread);
		pthread_mutex_unlock(&return_table_mutex);
	}
	return 0;
}

thread_t thread_self(void)
{
	if(thread_count != 0)
	{
		int id_core = get_idx_core();
		return CURRENT_THREAD->id;
	}
	return 1;
	
  //Why 1 ? If there is no thread
	//The created thread will be 2
	//The ending thread 0
	//And the current 1 
}

void thread_exit(void *retval)
{
	int id_core = get_idx_core();
	
  //Insert retval into the return_table
	if(retval != NULL)
	{
		pthread_mutex_lock(&return_table_mutex);
		htable__insert_int(return_table, CURRENT_THREAD->id, retval);
		pthread_mutex_unlock(&return_table_mutex);
	}
	
  //Finish exiting the thread by calling function to clean the thread
	thread_end_thread();
}

void put_back_joining_thread_of(thread_u* thread)
{
	int id_core = get_idx_core();
	pthread_mutex_lock(&join_queue_mutex);
	pthread_mutex_lock(&runqueue_mutex);
	
  //Get the join_list to know all the thread joining thread
	thread_u* tmp = NULL;
	fprintf(stderr, "Put back joiner of %d on core %d\n", thread->id, get_idx_core());
	
  //Put every joining thread back into the runqueu
	list__for_each(join_queue, tmp)
	{
		if(tmp->id_joining == thread->id)
		{
			list__remove(join_queue);
			tmp->is_joining = false;
			tmp->id_joining = -1;
			list__add_end(runqueue, tmp);
			sem_post(semaphore_runqueue);
			break;
		}
	}
	pthread_mutex_unlock(&join_queue_mutex);
	pthread_mutex_unlock(&runqueue_mutex);
}

void thread_catch_return(void * info)
{
	catch_return* cr = (catch_return*)info;
	void* ret = cr->function(cr->arg);
	thread_exit(ret);
}

int get_number_of_core(void)
{
	return 4; //sysconf(_SC_NPROCESSORS_ONLN);
}

int get_idx_core(void)
{
	int i, size = get_number_of_core();
	pthread_t self = pthread_self();
	for(i = 0; i < size; ++i)
		if(self == core[i].thread)
			return i;
	return -1;
}

void empty_handler(int s)
{
	printf("Call in empty handler core %d\n", get_idx_core());
}

void print_core_info(void)
{
	int i;
	int sum = 0;
	for(i = 0; i < get_number_of_core()*4 + 1; ++i)
		fprintf(stderr, "-");
	fprintf(stderr, "\n");
	for(i = 0; i < get_number_of_core(); ++i)
		sum += fprintf(stderr, "| %d ", i);
	sum += fprintf(stderr, "|\n");
	for(i = 0; i < sum-1; ++i)
		fprintf(stderr, "-");
	fprintf(stderr, "\n");
	for(i = 0; i < get_number_of_core(); ++i)
		if(core[i].current == NULL)
			fprintf(stderr, "| p ");
		else
			fprintf(stderr, "| %d ", core[i].current->id);
	fprintf(stderr, "|\n");
	for(i = 0; i < sum-1; ++i)
		fprintf(stderr, "-");
	fprintf(stderr, "\n");
}

void thread_change(int id_core)
{
	CURRENT_THREAD = NULL;
	if(CURRENT_CORE.previous != NULL && CURRENT_CORE.previous->to_clean)
	{
		fprintf(stderr, "Free %d on core %d\n", CURRENT_CORE.previous->id, id_core);
		VALGRIND_STACK_DEREGISTER(CURRENT_CORE.previous->valgrind_stackid);
		free(CURRENT_CORE.previous);
	}
	CURRENT_CORE.previous = NULL;
	
  //Unlock all ressources the current thread locked in the API
	if(CURRENT_CORE.unlock_runqueue)
		pthread_mutex_unlock(&runqueue_mutex);
	if(CURRENT_CORE.unlock_join_queue)
		pthread_mutex_unlock(&join_queue_mutex);
	
  //Now they are unlock, it's good
	CURRENT_CORE.unlock_runqueue = false;
	CURRENT_CORE.unlock_join_queue = false;
	
  //Get the next thread from the runqueu
	UNIGNORE_SIGNAL(SIGALRM);
	
  //Wait till there something in the runqueu
	sem_wait(semaphore_runqueue);
	IGNORE_SIGNAL(SIGALRM);
	
  //Lock the runqueu so even if there is many thread in the runqueu, only one will modify the runqueu
	pthread_mutex_lock(&runqueue_mutex);
	CURRENT_THREAD = list__remove_front(runqueue);
	pthread_mutex_unlock(&runqueue_mutex);
	
	  //Then switch to the thread context. No need to use swapcontext because the current context is not useful anymore
	if(CURRENT_THREAD != NULL)
	{
		//FIXME WHAT HAPPEN IF A SIG COME RIGHT NOW
		fprintf(stderr, "Start %d on core %d\n", CURRENT_THREAD->id, id_core);
		setcontext(&(CURRENT_THREAD->ctx));	
	}
	printf("Error CURRENT_THREAD is NULL\n");
	exit(0);
}

void thread_init_i(int i, thread_u* current_thread)
{
	
  //Initialise les variable du cœur
	core[i].unlock_runqueue = false;	
	core[i].unlock_join_queue = false;
	core[i].previous = NULL;
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
	if(i == 0)//Le cœur 0 à un traitement spécial car c'est lui qui exécute le code
	{
		core[0].thread = pthread_self();
		core[0].current = current_thread;
	}
	else
	{
			core[i].current = NULL;
			
  //Lancement du thread pthread (soit un cœur)
			pthread_create(&(core[i].thread), NULL, (void* (*)(void*))thread_change, (void*)(long int)i); 
	}
}
