#include "semaphore.h"
#include "isr.h"
#include "port.h"
#include <stddef.h>
#include <stdint.h>
#include "task.h"
#include "sched.h"


void sem_init(semaphore_t *s, int initial_count)
{
    s->count = (initial_count >= 0) ? initial_count : 0;
    s->wait_list = NULL;
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
