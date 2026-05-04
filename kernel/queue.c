#include "task.h"
#include "port.h"
#include "queue.h"
#include "alloc.h"
#include "critical.h"
#include "sched.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

struct queue{
    uint32_t item_size;    /* size of each item in bytes */
    uint32_t size;         /* requested maximum number of items the queue can hold (capacity) */
    uint32_t slots;        /* internal buffer slots (size + 1) used for ring buffer arithmetic */
    uint32_t head;         /* index of the head of the queue */
    uint32_t tail;         /* index of the tail of the queue */
    void *buffer;          /* pointer to the queue's data buffer */
    struct TaskControlBlock *send_wait; /* task waiting to send (queue full) */
    struct TaskControlBlock *recv_wait; /* task waiting to receive (queue empty) */
    struct queue *next; /* pointer to the next queue in the global list */
};

queue_t* g_queue_list; /* global list of all queues for cleanup (not implemented) */

static void wake_waiting_tasks(struct TaskControlBlock **wait_list) {
    struct TaskControlBlock *t = *wait_list;
    struct TaskControlBlock *prev = NULL;
    while (t) {
        struct TaskControlBlock *next = t->next; /* save next pointer since we may modify t->next when waking */
        if (t->state == TASK_WAITING && t->delay_ticks > 0) {
            if (--t->delay_ticks == 0) {
                t->state = TASK_READY;

                if(prev){
                    prev->next = next; /* remove from wait list */
                } else {
                    *wait_list = next; /* update head if first node was removed */
                }

                sched_add_task(t); /* add back to scheduler */
                t = next;
                continue;
            }
        }
        prev = t;
        t = next;
    }
}

void queue_tick_all(void) {
    queue_t *q = g_queue_list;
    while (q != NULL) {
        wake_waiting_tasks(&q->send_wait);
        wake_waiting_tasks(&q->recv_wait);
        q = q->next;
    }
}


static inline void add_to_queue_list(queue_t *queue) {
    queue_t *cur = g_queue_list;
    while(cur!=NULL){
        if(cur == queue) {
            return; /* already in list, avoid adding again */
        }
        if(cur->next == NULL) {
            break; /* reached end of list */
        }
        cur = cur->next;
    }
    if (cur == NULL) {
        g_queue_list = queue; /* list was empty, add as first element */
    } else {
        cur->next = queue; /* add to end of list */
    }
}

static inline void remove_from_queue_list(queue_t *queue) {
    queue_t* cur = g_queue_list;
    queue_t* prev = NULL;
    while(cur!=NULL){
        if(cur == queue) {
            if (prev == NULL) {
                g_queue_list = cur->next; /* removing first element */
            } else {
                prev->next = cur->next; /* bypass current element */
            }
            return; /* found and removed, exit */
        }
        prev = cur;
        cur = cur->next;
    }
    /* not found, nothing to remove */
}

/**
 * Create a queue. Allocates memory for the queue's buffer.
 * Returns 0 on success, -1 on failure.
 */
queue_t* queue_create(uint32_t item_size, uint32_t size){
    if (item_size == 0 || size == 0) {
        return NULL;
    }

    queue_t *queue = malloc(sizeof(queue_t));
    if (queue == NULL) {
        return NULL; /* Allocation failed */
    }

    if (queue_init(queue, item_size, size) != 0) {
        free(queue);
        return NULL; /* Initialization failed */
    }

    add_to_queue_list(queue); /* Add to global list if needed for cleanup (not implemented) */


    return queue; /* Success */
}

/**
 * Queue initialization. Must be called before using the queue.
 * 
 */
int queue_init(queue_t *queue, uint32_t item_size, uint32_t size){
    if (queue == NULL || item_size == 0 || size == 0) {
        return -1;
    }

    queue->item_size = item_size;
    queue->size = size; /* requested capacity */
    queue->slots = size + 1; /* internal slots = capacity + 1 */
    queue->head = 0;
    queue->tail = 0;

    size_t slots = (size_t)queue->slots;
    size_t total = (size_t)item_size * slots;
    if (slots != 0 && total / slots != (size_t)item_size) {
        return -1; /* overflow */
    }

    queue->buffer = malloc(total);
    if (queue->buffer == NULL) {
        return -1; /* Allocation failed */
    }

    return 0; /* Success */
}

/**
 * Push an item. Returns 0 on success, -1 if the queue is full.
 * 
 */
int queue_push(queue_t *queue, const void *item, uint32_t timeout_ms){
    while(1){

        uint32_t pm =  enter_critical();
        uint32_t next_index = (queue->tail + 1) % queue->slots;
        if (next_index == queue->head) {
            struct TaskControlBlock *current = scheduler_get_current();
            current->next = NULL;
            current->state = TASK_WAITING; /* set state to waiting */
            current->delay_ticks = timeout_ms; /* set timeout ticks */
            timeout_ms = 0; /* timeout handled by scheduler_tick */

            if (queue->send_wait == NULL) {
                queue->send_wait = current;
            } else {
                struct TaskControlBlock *tail = queue->send_wait;
                while (tail->next != NULL) tail = tail->next;
                tail->next = current;
            }
            sched_remove_task(current); /* remove from scheduler before blocking */
            exit_critical(pm);
            port_trigger_pendsv();
            continue; /* retry after being woken up */
        }
        memcpy((char*)queue->buffer + ((size_t)queue->tail * queue->item_size), item, queue->item_size);
        queue->tail = next_index;

        /* Wake-up any waiting tasks */
        if (queue->recv_wait != NULL) {
            struct TaskControlBlock *to_wake = queue->recv_wait;
            queue->recv_wait = to_wake->next; /* remove from wait list */
            to_wake->state = TASK_READY; /* set state to ready */
            to_wake->next = NULL; /* clear next pointer */
            to_wake->delay_ticks = 0; /* clear delay ticks */
            sched_add_task(to_wake); /* add back to scheduler */
        }
        
        exit_critical(pm);
        return 0; // Success
    }
}

/**
 * Pop an item. Returns 0 on success, -1 if the queue is empty.
 * The popped item is copied into the provided buffer.
 * 
 */
int queue_pop(queue_t *queue, void *item, uint32_t timeout_ms){
    while (1)
    {
        uint32_t pm = enter_critical();
        if (queue->head == queue->tail) {
            struct TaskControlBlock *current = scheduler_get_current();
            current->next = NULL;
            current->state = TASK_WAITING; /* set state to waiting */
            current->delay_ticks = timeout_ms; /* set timeout ticks */
            timeout_ms = 0; /* only block once, then return failure if still empty */
            if (queue->recv_wait == NULL) {
                queue->recv_wait = current;
            } else {
                struct TaskControlBlock *tail = queue->recv_wait;
                while (tail->next != NULL) tail = tail->next;
                tail->next = current;
            }
            sched_remove_task(current); /* remove from scheduler before blocking */
            exit_critical(pm);
            port_trigger_pendsv();
            continue; /* retry after being woken up */
        }

        memcpy(item, (char*)queue->buffer + ((size_t)queue->head * queue->item_size), queue->item_size);
        queue->head = (queue->head + 1) % queue->slots;

        /* Wake-up any waiting tasks */
        if (queue->send_wait != NULL) {
            struct TaskControlBlock *to_wake = queue->send_wait;
            queue->send_wait = to_wake->next; /* remove from wait list */
            to_wake->state = TASK_READY; /* set state to ready */
            to_wake->next = NULL; /* clear next pointer */
            to_wake->delay_ticks = 0; /* clear delay ticks */
            sched_add_task(to_wake); /* add back to scheduler */
        }

        exit_critical(pm);
        return 0; // Success
    }
}
