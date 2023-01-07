//Copyright 2022-2023 Raducanu George-Cristian 321CA
#include "so_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct so_task_t {
    int state; // 0 new; 1 ready; 2 running; 3 waiting; 4 terminated 
    int priority;
    int time_quantum;
    tid_t id_thread;
    so_handler *handler;
    sem_t semaphore;
    int io;
} so_task_t;

typedef struct scheduler_t {
    int global_time_quantum;
    so_task_t *current;
    so_task_t **priority_queue;
    so_task_t **running;
    int running_size;
    int queue_size;
    int is_initialized;
    unsigned int max_io;
} scheduler_t;

static scheduler_t my_scheduler;

void scheduler_call(void)
{
    if (my_scheduler.queue_size == 0) {
        if (my_scheduler.current->state != 4) {
            my_scheduler.current->time_quantum = my_scheduler.global_time_quantum;
            my_scheduler.current->state = 2;
            sem_post(&my_scheduler.current->semaphore);
        }
    	return;
    }

    so_task_t *next = my_scheduler.priority_queue[my_scheduler.queue_size - 1];

    if (my_scheduler.current == NULL) {
        my_scheduler.current = next;
        next->state = 2;
        my_scheduler.current->time_quantum = my_scheduler.global_time_quantum;
        my_scheduler.queue_size--;
        // semafor la ala sters
        sem_post(&next->semaphore);
        return;
    }

    if (my_scheduler.current->state == 3 || my_scheduler.current->state == 4) {
        my_scheduler.current->time_quantum = my_scheduler.global_time_quantum;
        my_scheduler.current = next;
        next->state = 2;
        my_scheduler.queue_size--;
        // semafor si la alea waiting/terminated
        sem_post(&next->semaphore);
        return;
    }

    if (my_scheduler.current->priority < next->priority) {
        // in caz de prioritate mai mica a celui curent
        my_scheduler.priority_queue[my_scheduler.queue_size++] = my_scheduler.current;
        for (int i = 0; i < my_scheduler.queue_size-1; ++i)
            for (int j = i+1; j < my_scheduler.queue_size; ++j)
                if (my_scheduler.priority_queue[i]->priority > my_scheduler.priority_queue[j]->priority) {
                    so_task_t *aux_swap = my_scheduler.priority_queue[i];
                    my_scheduler.priority_queue[i] = my_scheduler.priority_queue[j];
                    my_scheduler.priority_queue[j] = aux_swap;
                }
        my_scheduler.current = my_scheduler.priority_queue[my_scheduler.queue_size-1];
        next->state = 2;
        next->time_quantum = my_scheduler.global_time_quantum;
        my_scheduler.priority_queue[my_scheduler.queue_size - 1] = NULL;
        my_scheduler.queue_size--;
        sem_post(&next->semaphore);
        return;
    }

    if (my_scheduler.current->time_quantum <= 0) {
        if (my_scheduler.current->priority == next->priority) {
            my_scheduler.priority_queue[my_scheduler.queue_size++] = my_scheduler.current;
            for (int i = 0; i < my_scheduler.queue_size - 1; ++i)
                for (int j = i+1; j < my_scheduler.queue_size; ++j)
                    if (my_scheduler.priority_queue[i]->priority > my_scheduler.priority_queue[j]->priority) {
                        so_task_t *aux_swap = my_scheduler.priority_queue[i];
                        my_scheduler.priority_queue[i] = my_scheduler.priority_queue[j];
                        my_scheduler.priority_queue[j] = aux_swap;
                    }
            my_scheduler.current = my_scheduler.priority_queue[my_scheduler.queue_size-1];
            next->state = 2;
            next->time_quantum = my_scheduler.global_time_quantum;
            my_scheduler.queue_size--;
            sem_post(&my_scheduler.current->semaphore);
            return;
        }
        my_scheduler.current->time_quantum = my_scheduler.global_time_quantum;
    }
    sem_post(&my_scheduler.current->semaphore);
}

int so_init(unsigned int time_quantum, unsigned int io)
{
    if (my_scheduler.is_initialized != 0)
        return -1;
    if (io > 256 || time_quantum < 1)
        return -1;
    my_scheduler.is_initialized = 1;
    my_scheduler.max_io = io;
    my_scheduler.global_time_quantum = time_quantum;
    my_scheduler.current = NULL;
    my_scheduler.priority_queue = calloc(2000, sizeof(so_task_t *));
    my_scheduler.running = calloc(2000, sizeof(so_task_t *));
    return 0;
}

void helper(void *arguments)
{
    so_task_t *help = (so_task_t *)arguments;
    sem_wait(&help->semaphore);
    help->handler(help->priority);
    help->state = 4;
    scheduler_call();
}

tid_t so_fork(so_handler *func, unsigned int priority)
{
    so_task_t *newtask;

    if (!func)
        return ((tid_t)0);

    if (priority > 5)
        return ((tid_t)0);


    newtask = malloc(sizeof(so_task_t));

    //initializam noul thread
    newtask->id_thread = ((tid_t)0);
    newtask->state = 0;
    newtask->time_quantum = my_scheduler.global_time_quantum;
    newtask->priority = priority;
    newtask->handler = func;
    newtask->io = 256;
    sem_init(&newtask->semaphore, 0, 0);
    pthread_create(&newtask->id_thread, NULL, &helper, (void *)newtask);
    

    //acuma ar trebui sa inserez noul thread in coada
    my_scheduler.priority_queue[my_scheduler.queue_size] = newtask;
    my_scheduler.queue_size++;

    //il inserez si in cele running ca sa pot elibera dupa toate procesele
    my_scheduler.running[my_scheduler.running_size] = newtask;
    my_scheduler.running_size++;

    //sortez pentru a mentine "coada" mea ordonata
    
    for (int i = 0; i < my_scheduler.queue_size-1; ++i)
        for (int j = i+1; j < my_scheduler.queue_size; ++j)
            if (my_scheduler.priority_queue[i]->priority > my_scheduler.priority_queue[j]->priority) {
                so_task_t *aux_swap = my_scheduler.priority_queue[i];
                my_scheduler.priority_queue[i] = my_scheduler.priority_queue[j];
                my_scheduler.priority_queue[j] = aux_swap;
            }

    if (my_scheduler.current != NULL) {
            my_scheduler.priority_queue[my_scheduler.queue_size - 1]->state = 1;
            so_exec();
    } else {
        scheduler_call();
        my_scheduler.current->state = 2;
    }
    return newtask->id_thread;
}

int so_wait(unsigned int io)
{
    // daca portul pe care astept nu e valid
    if (io >= my_scheduler.max_io)
        return -1;
    // daca portul e valid
    my_scheduler.current->state = 3;
    my_scheduler.current->io = io;
    so_exec();
    return 0;
}

int so_signal(unsigned int io)
{
    // daca portul pe care astept nu e valid
    if (io >= my_scheduler.max_io)
        return -1;

    //numar cate procese asteapta dispozitivul cu portul io
    int count_io = 0;
    for (int k = 0; k < my_scheduler.running_size; ++k) {
        if (my_scheduler.running[k]->io == io && my_scheduler.running[k]->state == 3) {
            count_io++;
            my_scheduler.running[k]->state = 1;
            my_scheduler.running[k]->io = my_scheduler.max_io;
            my_scheduler.priority_queue[my_scheduler.queue_size] = my_scheduler.running[k];
            my_scheduler.queue_size++;
            for (int i = 0; i < my_scheduler.queue_size-1; ++i)
                for (int j = i+1; j < my_scheduler.queue_size; ++j)
                    if (my_scheduler.priority_queue[i]->priority > my_scheduler.priority_queue[j]->priority) {
                        so_task_t *aux_swap = my_scheduler.priority_queue[i];
                        my_scheduler.priority_queue[i] = my_scheduler.priority_queue[j];
                        my_scheduler.priority_queue[j] = aux_swap;
                    }
        }
    }
    so_exec();
    return count_io;
}

void so_exec(void)
{
    my_scheduler.current->time_quantum--;
    so_task_t *now_thread = my_scheduler.current;
    scheduler_call();
    sem_wait(&now_thread->semaphore);
}

void so_end(void)
{
    //distrug si eliberez totul
    //procese, memorie si semafoare
    for (int i = 0; i < my_scheduler.running_size; ++i)
        pthread_join(my_scheduler.running[i]->id_thread, NULL);

    for (int i = 0; i < my_scheduler.running_size; ++i) {
        sem_destroy(&my_scheduler.running[i]->semaphore);
        free(my_scheduler.running[i]);
    }
    free(my_scheduler.running);
    free(my_scheduler.priority_queue);
    my_scheduler.is_initialized = 0;
}
