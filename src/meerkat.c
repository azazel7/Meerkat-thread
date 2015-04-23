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
#include "global.h"
#include "runqueue.h"
#include "mutex.h"
#include "meerkat.h"

//La liste de tous les threads lancé.
List *runqueue = NULL;
mutex_t runqueue_mutex;
sem_t *semaphore_runqueue = NULL;

//Le nombre de thread lancé. Identique à list__get_size(runqueue).
static int thread_count = 0;

//Thread pour finir et nettoyer un thread fini
static thread_u ending_thread;

//Pour le timer, indique l'interval de temps (accédé uniquement par le pthread 0)
static struct itimerval timeslice;

//Tableau associatif qui répertorie quel thread attend quel autre
//La clef correspond a l'id du thread attendu et la donnée est la list (de type List*) des threads (des thread_u, pas des id) qui l'attendent
static htable* all_thread = NULL;
static mutex_t join_queue_mutex;

//Tableau associatif qui, pour chaque thread fini (clef = id du thread), lui associe sa valeur de retour si elle exist (via un appel à thread_exit)
static htable *return_table = NULL;

//Variable qui servira à donner un id à chaque thread
static int global_id = 0;

core_information *core = NULL;
int number_of_core = 1;

//TODO ajouter des verrous pour toutes les ressources partagées
//FIXME Les contexte partagent les buffer de printf !!!!!!!!!
//L'appel à exit kill tout le monde ...
//faire un return dans le main kill tout le monde
//TODO add in core_information something to say it's already in thread change (for signal)

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
void put_back_joining_thread_of(thread_u * thread);

//Si la fonction du thread fait un return, thread_catch_return récupérera la valeur et la mettra dans return_table pour que les thread qui join puissent l'avoir
void thread_catch_return(void *info);

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

void apply_join(thread_u* thread);

void apply_put_back(thread_u* thread);

int thread_init(void)
{
	int i;
	FPRINTF("First\n");

	number_of_core = 4;	//sysconf(_SC_NPROCESSORS_ONLN);

	//Ajoute les gestionnaire de signaux
	signal(SIGALRM, thread_handler);
	signal(SIGVTALRM, thread_handler);
	core = malloc(sizeof(core_information) * number_of_core);
	if(core == NULL)
		return -1;

	//Initialise les structures dans lesquelles on va ranger les donnée
	runqueue = list__create();
	all_thread = htable__create_int(true);
	return_table = htable__create_int(true);

	//Initialise les mutex
	mutex_init(&runqueue_mutex);
	mutex_init(&join_queue_mutex);

	//Initialise et créer la sémaphore
	sem_init(semaphore_runqueue, 0, 0);

	//Prépare le temps pour le timer
	timeslice.it_value.tv_sec = 2;
	timeslice.it_value.tv_usec = 0;

	//On re-init nous-même le timer pour ne pas compter le temps de re-schedul
	timeslice.it_interval.tv_sec = 0;
	timeslice.it_interval.tv_usec = 0;

	if(getcontext(&(ending_thread.ctx)) < 0)
		exit(-1);
	//thread_count == 0 => slot == 0
	ending_thread.ctx.uc_stack.ss_sp = ending_thread.stack;
	ending_thread.ctx.uc_stack.ss_size = sizeof(ending_thread.stack);
	ending_thread.ctx.uc_link = NULL;

	//Information sur le thread pour l'api
	ending_thread.to_clean = false;
	ending_thread.is_joining = false;

	//Un seul thread pour le moment, donc pas besoin de verrou
	ending_thread.id = global_id++;
	VALGRIND_STACK_REGISTER(ending_thread.ctx.uc_stack.ss_sp, ending_thread.ctx.uc_stack.ss_sp + SIZE_STACK);

	//créer le contexte pour le contexte de netoyage
	makecontext(&(ending_thread.ctx), free_ressources, 1, NULL);
	++thread_count;
	thread_u *current_thread = (thread_u *) malloc(sizeof(thread_u));
	if(current_thread == NULL)
	{
		thread_count = 0;
		return -1;
	}
	current_thread->to_clean = false;
	current_thread->is_joining = false;
	current_thread->joiner = NULL;

	//Un seul thread pour le moment, donc pas besoin de verrou
	current_thread->id = global_id++;
	current_thread->ctx.uc_link = NULL;
	current_thread->cr.function = NULL;
	current_thread->cr.arg = NULL;
	current_thread->id_joining = -1;

	htable__insert_int(all_thread, current_thread->id, current_thread);

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
	thread_u *new_thread = (thread_u *) malloc(sizeof(thread_u));
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
	new_thread->ctx.uc_link = NULL; //&ending_thread.ctx;

	//Infos supplémentaire sur le thread
	new_thread->to_clean = false;
	new_thread->is_joining = false;
	new_thread->joiner = NULL;

	//Configure l'argument que l'on va donner à thread_catch_return qui exécutera start_routine
	new_thread->cr.function = start_routine;
	new_thread->cr.arg = arg;
	new_thread->id_joining = -1;

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
	new_thread->id = __sync_fetch_and_add(&global_id, 1);

	//Attend d'avoir initialiser l'id avant de le donner
	if(newthread != NULL)
		//Si l'utilisateur ne veut pas se souvenir du thread, on ne lui dit pas
		*newthread = new_thread->id;

	__sync_add_and_fetch(&thread_count, 1);

	htable__insert_int(all_thread, new_thread->id, new_thread);
	//new_thread correspond au thread courant
	mutex_lock(&runqueue_mutex);
	list__add_end(runqueue, new_thread);
	sem_post(semaphore_runqueue);
	FPRINTF("Create new thread %d on core %d\n", new_thread->id, get_idx_core());
	mutex_unlock(&runqueue_mutex);
	return 0;
}

void thread_end_thread()
{
	int id_core = get_idx_core();
	FPRINTF("Exit thread %d on core %d\n", CURRENT_THREAD->id, id_core);

	//Informe the scheduler he will have to clean this thread
	CURRENT_THREAD->to_clean = true;

	//One thread less
	bool is_no_more_thread = __sync_sub_and_fetch(&thread_count, 1) <= 0;

	//If no more thread, we are the last
	if(is_no_more_thread)
	{
		FPRINTF("Finishing by %d on core %d\n", CURRENT_THREAD->id, id_core);

		// On appelle freeRessources afin de libérer les variables globales
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
	FPRINTF("Pause %d on core %d\n", CURRENT_CORE.previous->id, id_core);

	//If the current thread is not finish, we push him back into the runqueu
	if(CURRENT_CORE.previous != NULL && CURRENT_CORE.previous->to_clean)
	{
		//So the current thread is no more active which mean less deadlock with the join
		//The join will hopefully no see it
		//the return value is already in the return table
		htable__remove_int(all_thread, CURRENT_CORE.previous->id);

		//Bring back in the runqueu threads that have joined the current_thread
		put_back_joining_thread_of(CURRENT_CORE.previous);

		//Do not free here, cause you will free the stack on which you are executed.
		//It's like cut the branch you sit on. It's stupid.
		FPRINTF("Plan deletion %d on core %d\n", CURRENT_CORE.previous->id, id_core);

		//There is no more CURRENT_CORE.previous because we free it
		current_thread_dead = true;
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

	//TODO ré-armer le signal (mais pas sûr)
	//If we are the first core, we are the only one to receive this signal so warn the others
	if(id_core == 0)
		for(i = 1; i < number_of_core; ++i)
			pthread_kill(core[i].thread, SIGALRM);

	//If the current core doesn't execute something, it's sleeping, so it already is in thread_schedul
	//TODO change that by an attribut
	if(CURRENT_THREAD != NULL)
		thread_schedul();
}

int thread_yield(void)
{
	if(thread_count != 0)
	{
		thread_schedul();
	}
	return 0;
}

int thread_join(thread_t thread, void **retval)
{
	int id_core = get_idx_core();
	if(CURRENT_THREAD == NULL || CURRENT_THREAD->id == thread)
		return -1;
	FPRINTF("%d try to join %d on core %d\n", CURRENT_THREAD->id, thread, id_core);

	//Check if the thread exist in the runqueu
	//Put also join_queue_mutex because if put_back_joining_thread_of is called after we've checked it could be a problem
	
	//Find the list of all the threads joining thread
	CURRENT_THREAD->is_joining = true;
	CURRENT_THREAD->id_joining = thread;

	//Put information for the scheduler so he will know how to deal with it
	FPRINTF("%d join %d on core %d\n", CURRENT_THREAD->id, thread, id_core);

	//switch of process
	thread_schedul();

	//We are back, just check the return value of thread and go back to work
	if(retval != NULL)
		*retval = htable__remove_int(return_table, thread);
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
		htable__insert_int(return_table, CURRENT_THREAD->id, retval);
	//Finish exiting the thread by calling function to clean the thread
	thread_end_thread();
}

void put_back_joining_thread_of(thread_u * thread)
{
	//Get the join_list to know all the thread joining thread
	thread_u *tmp = NULL;
	FPRINTF("Put back joiner of %d on core %d\n", thread->id, get_idx_core());
	if(thread->joiner != NULL)
	{
		thread->joiner->is_joining = false;
		thread->joiner->id_joining = -1;
		add_thread_to_runqueue(get_idx_core(), thread->joiner);
	}
}

void thread_catch_return(void *info)
{
	catch_return *cr = (catch_return *) info;
	void *ret = cr->function(cr->arg);
	thread_exit(ret);
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
	if(CURRENT_THREAD != NULL && CURRENT_THREAD->to_clean)
	{
		FPRINTF("Free %d on core %d\n", CURRENT_THREAD->id, id_core);
		VALGRIND_STACK_DEREGISTER(CURRENT_THREAD->valgrind_stackid);
		free(CURRENT_THREAD);
	}
	else if(CURRENT_THREAD != NULL && !CURRENT_THREAD->is_joining)	//FIXME don't know why, but if you use id_joining, it doesn't work.
	{
		add_thread_to_runqueue(id_core, CURRENT_THREAD);
	}
	else if(CURRENT_THREAD != NULL && CURRENT_THREAD->is_joining)	//FIXME don't know why, but if you use id_joining, it doesn't work.
	{
		void* found = htable__find_and_apply(all_thread, (void*)(long long int)CURRENT_THREAD->id_joining, (void (*)(void*))apply_join);
		if(!found)
			add_thread_to_runqueue(id_core, CURRENT_THREAD);
	}
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
	printf("Error CURRENT_THREAD is NULL\n");
	exit(0);
}

void thread_init_i(int i, thread_u * current_thread)
{

	//Initialise les variable du cœur
	core[i].unlock_runqueue = false;
	core[i].unlock_join_queue = false;
	core[i].previous = NULL;
	core[i].runqueue = list__create();	
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
	/*if(core != NULL)*/
		/*setcontext(&(ending_thread.ctx));*/
}

void free_ressources(void)
{
	int id_core = get_idx_core();
	int i;

	//Free every thing
	list__destroy(runqueue);
	mutex_destroy(&runqueue_mutex);
	sem_close(semaphore_runqueue);
	sem_destroy(semaphore_runqueue);

	//XXX destroy my id into all_thread ?
	htable__remove_int(return_table, CURRENT_THREAD->id);
	htable__destroy(return_table);
	htable__destroy(all_thread);
	free(CURRENT_THREAD);
	for(i = 0; i < number_of_core; ++i)
	{
		while(core[i].previous != NULL && core[i].current != NULL);
		VALGRIND_STACK_DEREGISTER(core[i].valgrind_stackid);
	}
	free(core);
	FPRINTF("Finished by core %d\n", id_core);
}

void apply_join(thread_u* thread)
{
	int id_core = get_idx_core();
	if(thread->joiner == NULL)
		thread->joiner = CURRENT_CORE.previous;
}
