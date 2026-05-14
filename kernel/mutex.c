#include "mutex.h"
#include "isr.h"
#include "port.h"
#include <stddef.h>
#include <stdint.h>
#include "task.h"
#include "sched.h"

struct mutex {
    struct TaskControlBlock *owner;     /* task currently holding the mutex, NULL if free */
    struct TaskControlBlock *wait_list; /* singly-linked list of waiting tasks (via ->next) */
};

mutex_t* mutex_create(void)
{
    mutex_t *m = (mutex_t *)malloc(sizeof(mutex_t));
    if (m == NULL) {
        return NULL;
    }
    m->owner = NULL;
    m->wait_list = NULL;
    return m;
}

void mutex_lock(mutex_t *m)
{
    uint32_t pm = enter_critical();
    if(m->owner == scheduler_get_current()){
        // current task already owns the mutex
        exit_critical(pm);
        return;
    }
    if(m->owner == NULL){
        m->owner = scheduler_get_current();
        exit_critical(pm);
        return;
    }
    struct TaskControlBlock *cur = scheduler_get_current();
    cur->state = TASK_WAITING;
    cur->next = m->wait_list;
    m->wait_list = cur;
    exit_critical(pm);
    port_trigger_pendsv();
    return;
}

void mutex_unlock(mutex_t *m)
{
    uint32_t pm = enter_critical();
    struct TaskControlBlock *cur = scheduler_get_current();
    if(m->owner != cur){
        // Error: current task does not own the mutex
        exit_critical(pm);
        return;
    }
    if(m->wait_list != NULL){
        struct TaskControlBlock *waiter = m->wait_list;
        waiter->state = TASK_READY;
        m->owner = waiter;
        m->wait_list = waiter->next;
    } else {
        m->owner = NULL;
    }
    exit_critical(pm);
    port_trigger_pendsv();
    return;
}
