#include "so_scheduler.h"
#include <stdlib.h>
#include <string.h>

#define READY 1
#define WAITING 2

struct thread
{
    so_handler *func;
    HANDLE sem;
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

static unsigned int quantum = -1;
static unsigned int io_max;

static struct node *first = NULL;
static int next_index = 0;
static HANDLE end_program;

DWORD WINAPI thread_func(LPVOID arg)
{    
    struct thread *t = (struct thread*)arg;
    struct node *n, *prev;
    
    /* wait for thread to be scheduled */
    WaitForSingleObject(t->sem, INFINITE);
    
    /* execute instructions */
    t->func(t->priority);
    
    /* after executing all instructions,
     * remove struct thread from list */
    n = first;
    prev = NULL;
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
            CloseHandle(n->thr.sem);
            free(n);
            break;
        }
        prev = n;
        n = n->next;
    }
    if (first == NULL)
    {
        /* this was the last thread */
        ReleaseSemaphore(end_program, 1, NULL);
        return 0;
    }
    /* schedule other thread with highest priority */
    n = first;
    while(n != NULL && n->thr.state == WAITING)
    {
        n = n->next;
    }
    if (n != NULL)
    {
        ReleaseSemaphore(n->thr.sem, 1, NULL);
    }
    return 0;
}

int so_init(unsigned int time_quantum, unsigned int io)
{
    if (quantum != (unsigned int)-1)
        return -1;
    end_program = CreateSemaphore(NULL, 1L, 1L, NULL);
    if (end_program == NULL)
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
    WaitForSingleObject(end_program, INFINITE);
    quantum = (unsigned int)-1;
    CloseHandle(end_program);
}

tid_t so_fork(so_handler *func, unsigned int priority)
{
    DWORD dwRet;
    struct node *current, *prev, *n, *new_node, *new_pos, *to_schedule;
    struct thread new_thread;
    tid_t tid;

    if (func == NULL || priority > SO_MAX_PRIO)
        return INVALID_TID;
    /* if it's the initial fork, decrement the end_program semaphore */
    dwRet = WaitForSingleObject(end_program, 0);
    if (dwRet == WAIT_FAILED)
        return INVALID_TID;
    /* determine which thread made this call */
    current = first;
    prev = NULL;
    if (current != NULL)
    {
        while (current->thr.state == WAITING)
        {
            prev = current;
            current = current->next;
        }
    }
    /* create new struct thread and call CreateThread with thread_func */
    new_thread.func = func;
    new_thread.sem =  CreateSemaphore(NULL, 0L, 1L, NULL);
    if (new_thread.sem == NULL)
        return INVALID_TID;
    new_thread.index = next_index++;
    new_thread.priority = priority;
    new_thread.count = 0;
    new_thread.state = READY;
    new_thread.io = io_max + 1;
    /* insert new thread in the thread list, according to its priority */
    n = first;
    if (n == NULL)
    {
        /* initial fork, called by the system */
        first = malloc(sizeof(struct node));
        if (first == NULL)
            return INVALID_TID;
        memcpy(&(first->thr), &new_thread, sizeof(struct thread));
        first->next = NULL;
        /* create thread */
        if (CreateThread(NULL, 0, thread_func, (LPVOID)first, 0, &tid) == NULL)
            return INVALID_TID;
        /* start current thread execution, since it is the only one */
        if (ReleaseSemaphore(first->thr.sem, 1, NULL) == FALSE)
            return INVALID_TID;
        return tid;
    }
    else /* if not initial fork */
    {
        while(n->next != NULL && n->next->thr.priority >= priority)
        {
            n = n->next;
        }
        new_node = malloc(sizeof(struct node));
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
        if (CreateThread(NULL, 0, thread_func, (LPVOID)new_node, 0, &tid) == NULL)
            return INVALID_TID;
        /* if newly created thread has a higher priority than the current thread,
         * current thread is preempted and the new thread starts */
        if (priority > current->thr.priority)
        {
            current->thr.count = 0;
            if (ReleaseSemaphore(new_node->thr.sem, 1, NULL) == FALSE)
                return INVALID_TID;
            if (WaitForSingleObject(current->thr.sem, INFINITE) == WAIT_FAILED)
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
                new_pos = current;
                to_schedule = NULL;
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
                    if (ReleaseSemaphore(to_schedule->thr.sem, 1, NULL) == FALSE)
                        return INVALID_TID;
                    if (WaitForSingleObject(current->thr.sem, INFINITE) == WAIT_FAILED)
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
    struct node *n = first, *prev = NULL, *new_pos, *to_schedule;
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
        new_pos = n;
        to_schedule = NULL;
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
                ReleaseSemaphore(to_schedule->thr.sem, 1, NULL);
                WaitForSingleObject(n->thr.sem, INFINITE);
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
    struct node *current, *n;
    
    if (io > io_max)
        return -1;
    /* ready thread with highest priority made this call */
    current = first;
    while(current->thr.state == WAITING)
    {
        current = current->next;
    }
    current->thr.count = 0;
    current->thr.state = WAITING;
    current->thr.io = io;
    /* schedule other thread with highest priority */
    n = current->next;
    while (n->thr.state == WAITING)
    {
        n = n->next;
    }
    if (ReleaseSemaphore(n->thr.sem, 1, NULL) == FALSE)
        return -1;
    if (WaitForSingleObject(current->thr.sem, INFINITE) == WAIT_FAILED)
        return -1;
    return 0;
}

int so_signal(unsigned int io)
{
    struct node *current = NULL, *n = first, *prev = NULL, *to_schedule = NULL;
    int woken = 0;
    
    if (io > io_max)
        return -1;
    /* change state to READY for all threads that were waiting for this event/io
     * if any thread with higher priority was signaled, current thread is preempted
     * else if current thread still has time left, he continues execution, so move it to the left of the list
     * else move it to the right of the list */
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
            if (ReleaseSemaphore(to_schedule->thr.sem, 1, NULL) == FALSE)
                return -1;
            if (WaitForSingleObject(current->thr.sem, INFINITE) == WAIT_FAILED)
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
                if (ReleaseSemaphore(to_schedule->thr.sem, 1, NULL) == FALSE)
                    return -1;
                if (WaitForSingleObject(current->thr.sem, INFINITE) == WAIT_FAILED)
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