#include "semaphore.h"
#include "isr.h"
#include "port.h"
#include <stddef.h>
#include <stdint.h>
#include "task.h"
#include "sched.h"
#include "alloc.h"

struct semaphore{
    int count;                          /* current count (>= 0) */
    struct TaskControlBlock *wait_list; /* tasks blocked in sem_wait() */
};

semaphore_t *sem_create(int initial_count)
{
    semaphore_t *s = (semaphore_t *)malloc(sizeof(semaphore_t));
    if (s == NULL) {
        return NULL;
    }
    s->count = (initial_count >= 0) ? initial_count : 0;
    s->wait_list = NULL;
    return s;
}

void sem_take(semaphore_t *s)
{
    uint32_t pm = enter_critical();
    if(s->count > 0 ){
        s->count--;
        exit_critical(pm);
        return;
    }
    struct TaskControlBlock *cur = scheduler_get_current();
    cur->state = TASK_WAITING;
    cur->next = s->wait_list;
    s->wait_list = cur;
    exit_critical(pm);
    port_trigger_pendsv();
    return;
}

void sem_give(semaphore_t *s)
{
    uint32_t pm = enter_critical();
    if(s->wait_list != NULL){
        struct TaskControlBlock *waiter = s->wait_list;
        waiter->state = TASK_READY;
        s->wait_list = waiter->next;
        exit_critical(pm);
        port_trigger_pendsv();
        return;
    }
    s->count++;
    exit_critical(pm);
    return;
}
