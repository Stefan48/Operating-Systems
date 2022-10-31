#include "so_scheduler.h"
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>

#define READY 1
#define WAITING 2

struct thread
{
    so_handler *func;
    sem_t sem;
    int index;
    unsigned int priority;
    unsigned int count;
    unsigned int state;
    unsigned int io;
};

struct node
{
    struct thread thr;
    struct node *next;
};

struct thread_id
{
    tid_t tid;
    struct thread_id *next;
};

static unsigned int quantum = -1;
static unsigned int io_max;

static struct node *first = NULL;
static struct thread_id *joinable = NULL;
static int next_index = 0;
static sem_t end_program;


void *thread_func(void *arg)
{    
    struct thread *t = (struct thread*)arg;
    
    /* wait for thread to be scheduled */
    sem_wait(&(t->sem));
    
    /* execute instructions */
    t->func(t->priority);
    
    /* after executing all instructions,
     * remove struct thread from list */
    struct node *n = first, *prev = NULL;
    while (n != NULL)
    {
        if (n->thr.index == t->index)
        {
            if (prev == NULL)
            {
                first = n->next;
            }
            else
            {
                prev->next = n->next;
            }
            sem_destroy(&(n->thr.sem));
            free(n);
            break;
        }
        prev = n;
        n = n->next;
    }
    if (first == NULL)
    {
        /* this was the last thread */
        sem_post(&end_program);
        pthread_exit(NULL);
    }
    /* schedule other thread with highest priority */
    n = first;
    while(n != NULL && n->thr.state == WAITING)
    {
        n = n->next;
    }
    if (n != NULL)
    {
        sem_post(&(n->thr.sem));
    }
    pthread_exit(NULL);
}

int so_init(unsigned int time_quantum, unsigned int io)
{
    if (quantum != (unsigned int)-1)
        return -1;
    if (sem_init(&end_program, 0, 1) == -1)
        return -1;
    if (time_quantum == 0 || io > SO_MAX_NUM_EVENTS)
        return -1;
    quantum = time_quantum;
    io_max = io - 1;
    return 0;
}

void so_end(void)
{
    if (quantum == (unsigned int)-1)
        return;
    sem_wait(&end_program);
    /* join all created threads */
    struct thread_id *t = joinable, *next = NULL;
    while (t != NULL)
    {
        next = t->next;
        pthread_join(t->tid, NULL);
        free(t);
        t = next;
    }
    quantum = (unsigned int)-1;
    sem_destroy(&end_program);
}

tid_t so_fork(so_handler *func, unsigned int priority)
{
    if (func == NULL || priority > SO_MAX_PRIO)
        return INVALID_TID;
    /* if it's the initial fork, decrement the end_program semaphore */
    int end;
    if (sem_getvalue(&end_program, &end) == -1)
        return INVALID_TID;
    if (end == 1)
    {
        if (sem_wait(&end_program) == -1)
            return INVALID_TID;
    }
    /* determine which thread made this call */
    struct node *current = first, *prev = NULL;
    if (current != NULL)
    {
        while (current->thr.state == WAITING)
        {
            prev = current;
            current = current->next;
        }
    }
    /* create new struct thread and call pthread_create with thread_func */
    struct thread new_thread;
    new_thread.func = func;
    if (sem_init(&new_thread.sem, 0, 0) == -1)
        return INVALID_TID;
    new_thread.index = next_index++;
    new_thread.priority = priority;
    new_thread.count = 0;
    new_thread.state = READY;
    new_thread.io = io_max + 1;
    tid_t tid;
    /* insert new thread in the thread list, according to its priority */
    struct node *n = first;
    if (n == NULL)
    {
        /* initial fork, called by the system */
        first = malloc(sizeof(struct node));
        if (first == NULL)
            return INVALID_TID;
        memcpy(&(first->thr), &new_thread, sizeof(struct thread));
        first->next = NULL;
        /* create thread */
        if (pthread_create(&tid, NULL, thread_func, (void*)first))
            return INVALID_TID;
        joinable = malloc(sizeof(struct thread_id));
        if (joinable == NULL)
            return INVALID_TID;
        joinable->tid = tid;
        joinable->next = NULL;
        /* start current thread execution, since it is the only one */
        if (sem_post(&(first->thr.sem)) == -1)
            return INVALID_TID;
        return tid;
    }
    else /* if not initial fork */
    {
        while(n->next != NULL && n->next->thr.priority >= priority)
        {
            n = n->next;
        }
        struct node *new_node = malloc(sizeof(struct node));
        if (new_node == NULL)
            return INVALID_TID;
        memcpy(&(new_node->thr), &new_thread, sizeof(struct thread));
        if (priority > n->thr.priority)
        {
            first = new_node;
            new_node->next = n;
        }
        else
        {
            new_node->next = n->next;
            n->next = new_node;
        }
        /* create thread */
        if (pthread_create(&tid, NULL, thread_func, (void*)new_node))
            return INVALID_TID;
        struct thread_id *t = joinable;
        while (t->next != NULL)
        {
            t = t->next;
        }
        t->next = malloc(sizeof(struct thread_id));
        t = t->next;
        if (t == NULL)
            return INVALID_TID;
        t->tid = tid;
        t->next = NULL;
        /* if newly created thread has a higher priority than the current thread,
         * current thread is preempted and the new thread starts */
        if (priority > current->thr.priority)
        {
            current->thr.count = 0;
            if (sem_post(&(new_node->thr.sem)) == -1)
                return INVALID_TID;
            if (sem_wait(&(current->thr.sem)) == -1)
                return INVALID_TID;
            return tid;
        }
        else
        {
            current->thr.count++;
            if (current->thr.count == quantum)
            {
                current->thr.count = 0;
                /* if time quantum expired, move current thread
                 * so that it is the last of the threads with this priority */
                struct node *new_pos = current, *to_schedule = NULL;
                while (new_pos->next != NULL && new_pos->next->thr.priority == current->thr.priority)
                {
                    new_pos = new_pos->next;
                    if (to_schedule == NULL)
                    {
                        to_schedule = new_pos;
                    }
                }
                if (new_pos != current)
                {
                    if (prev == NULL)
                    {
                        first = current->next;
                        current->next = new_pos->next;
                        new_pos->next = current;
                    }
                    else
                    {
                        prev->next = current->next;
                        current->next = new_pos->next;
                        new_pos->next = current;
                    }
                    if (sem_post(&(to_schedule->thr.sem)) == -1)
                        return INVALID_TID;
                    if (sem_wait(&(current->thr.sem)) == -1)
                        return INVALID_TID;
                    return tid;
                }
                /* else, there are no other threads with same priority that should be scheduled
                 * so current thread continues execution */
            }
            /* else, current thread still has available time on the processor,
             * so it continues execution */
        }    
    }
    return tid;
}

void so_exec(void)
{
    /* ready thread with highest priority made this call */
    struct node *n = first, *prev = NULL;
    while(n->thr.state == WAITING)
    {
        prev = n;
        n = n->next;
    }
    n->thr.count++;
    if (n->thr.count == quantum)
    {
        n->thr.count = 0;
        /* if time quantum expired, move current thread
         * so that it is the last of the threads with this priority */
        struct node *new_pos = n, *to_schedule = NULL;
        while (new_pos->next != NULL && new_pos->next->thr.priority == n->thr.priority)
        {
            new_pos = new_pos->next;
            if (to_schedule == NULL && new_pos->thr.state == READY)
            {
                to_schedule = new_pos;
            }
        }
        if (new_pos != n)
        {
            if (prev == NULL)
            {
                first = n->next;
                n->next = new_pos->next;
                new_pos->next = n;
            }
            else
            {
                prev->next = n->next;
                n->next = new_pos->next;
                new_pos->next = n;
            }
            if (to_schedule != NULL)
            {
                /* schedule other thread with same priority */
                sem_post(&(to_schedule->thr.sem));
                sem_wait(&(n->thr.sem));
                return;
            }
        }
        /* else, there are no other threads with same priority that should be scheduled
         * so current thread continues execution */
    }
    /* else, current thread still has available time on the processor,
     * so it continues execution */
}

int so_wait(unsigned int io)
{
    if (io > io_max)
        return -1;
    /* ready thread with highest priority made this call */
    struct node *current = first;
    while(current->thr.state == WAITING)
    {
        current = current->next;
    }
    current->thr.count = 0;
    current->thr.state = WAITING;
    current->thr.io = io;
    /* schedule other thread with highest priority */
    struct node *n = current->next;
    while (n->thr.state == WAITING)
    {
        n = n->next;
    }
    if (sem_post(&(n->thr.sem)) == -1)
        return -1;
    if (sem_wait(&(current->thr.sem)) == -1)
        return -1;
    return 0;
}

int so_signal(unsigned int io)
{
    if (io > io_max)
        return -1;
    /* change state to READY for all threads that were waiting for this event/io
     * if any thread with higher priority was signaled, current thread is preempted
     * else if current thread still has time left, he continues execution, so move it to the left of the list
     * else move it to the right of the list */
    struct node *current = NULL, *n = first, *prev = NULL, *to_schedule = NULL;
    int woken = 0;
    while (n != NULL)
    {
        if (n->thr.state == WAITING)
        {
            if (n->thr.io == io)
            {
                woken++;
                n->thr.state = READY;
                n->thr.io = io_max + 1;
                if (to_schedule == NULL)
                {
                    to_schedule = n;
                }
            }
        }
        else
        {
            /* first ready node found is the current node (which called so_signal) */
            if (current == NULL)
            {
                current = n;
            }
            else if (to_schedule == NULL)
            {
                to_schedule = n;
            }
        }
        if (current == NULL)
        {
            prev = n;
        }
        n = n->next;
    }
    if (to_schedule == NULL)
    {
        /* current thread continues execution */
        current->thr.count++;
        if (current->thr.count == quantum)
        {
            current->thr.count = 0;
        }
        return woken;
    }
    else
    {
        if (to_schedule->thr.priority > current->thr.priority)
        {
            /* schedule thread with higher priority */
            current->thr.count = 0;
            /* move current thread to the right in list */
            n = current;
            while(n->next != NULL && n->next->thr.priority == current->thr.priority)
            {
                n = n->next;
            }
            if (n != current)
            {
                if (prev == NULL)
                {
                    first = current->next;
                    current->next = n->next;
                    n->next = current;
                }
                else
                {
                    prev->next = current->next;
                    current->next = n->next;
                    n->next = current;
                }
            }
            /* schedule signaled thread */
            if (sem_post(&(to_schedule->thr.sem)) == -1)
                return -1;
            if (sem_wait(&(current->thr.sem)) == -1)
                return -1;
            return woken;
        }
        else if (current->thr.priority > to_schedule->thr.priority)
        {
            /* current thread continues execution */
            current->thr.count++;
            if (current->thr.count == quantum)
            {
                current->thr.count = 0;
            }
            return woken;
        }
        else /* if (current->thr.priority == to_schedule->thr.priority) */
        {
            current->thr.count++;
            if (current->thr.count == quantum)
            {
                current->thr.count = 0;
                /* move current thread to the right in list */
                n = current;
                while(n->next != NULL && n->next->thr.priority == current->thr.priority)
                {
                    n = n->next;
                }
                if (n != current)
                {
                    if (prev == NULL)
                    {
                        first = current->next;
                        current->next = n->next;
                        n->next = current;
                    }
                    else
                    {
                        prev->next = current->next;
                        current->next = n->next;
                        n->next = current;
                    }
                }
                /* quantum expired, schedule other thread */
                if (sem_post(&(to_schedule->thr.sem)) == -1)
                    return -1;
                if (sem_wait(&(current->thr.sem)) == -1)
                    return -1;
                return woken;
            }
            else
            {
                /* move current thread to the left in list
                 * (since it will continue execution even if other threads with same priority were signaled) */
                if (current != first)
                {
                    if (current->thr.priority == first->thr.priority)
                    {
                        prev->next = current->next;
                        current->next = first;
                        first = current;
                    }
                    else
                    {
                        n = first;
                        while (n->next != NULL && n->next->thr.priority > current->thr.priority)
                        {
                            n = n->next;
                        }
                        prev->next = current->next;
                        current->next = n->next;
                        n->next = current;
                    }
                }
                return woken;
            }
        }
    }
    return woken;
}
