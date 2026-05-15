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
    struct TaskControlBlock *send_wait; /* tasks blocked on push (queue full) */
    struct TaskControlBlock *recv_wait; /* tasks blocked on pop  (queue empty) */
    /* Wait lists use TCB.wait_next — separate from TCB.next (priority ring) */
};

static int queue_init(queue_t *queue, uint32_t item_size, uint32_t size){
    if (queue == NULL || item_size == 0 || size == 0) {
        return -1;
    }

    queue->item_size = item_size;
    queue->size = size;
    queue->slots = size + 1;
    queue->head = 0;
    queue->tail = 0;
    queue->send_wait = NULL;
    queue->recv_wait = NULL;

    size_t slots = (size_t)queue->slots;
    size_t total = (size_t)item_size * slots;
    if (slots != 0 && total / slots != (size_t)item_size) {
        return -1;
    }

    queue->buffer = malloc(total);
    if (queue->buffer == NULL) {
        return -1;
    }

    return 0;
}

queue_t* z_impl_queue_create(uint32_t item_size, uint32_t size){
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

    return queue; /* Success */
}

/**
 * Push an item. Returns 0 on success, -1 if the queue is full or timed out.
 */
int z_impl_queue_push(queue_t *queue, const void *item, uint32_t timeout_ms)
{
    uint32_t pm = enter_critical();
    for (;;) {
        uint32_t next_index = (queue->tail + 1) % queue->slots;
        if (next_index != queue->head) {
            /* Space available — copy item and wake one receiver. */
            memcpy((char *)queue->buffer +
                   (size_t)queue->tail * queue->item_size, item, queue->item_size);
            queue->tail = next_index;
            if (queue->recv_wait != NULL) {
                struct TaskControlBlock *to_wake = queue->recv_wait;
                queue->recv_wait           = to_wake->wait_next;
                to_wake->wait_next         = NULL;
                to_wake->delay_ticks       = 0;
                to_wake->blocking_wait_list = NULL;
                exit_critical(pm);
                scheduler_wake_task(to_wake, 0);
                return 0;
            }
            exit_critical(pm);
            return 0;
        }
        /* Queue full. */
        if (timeout_ms == 0) {
            exit_critical(pm);
            return -1;
        }
        struct TaskControlBlock *cur = scheduler_get_current();
        cur->flags    &= ~TASK_FLAG_TIMED_OUT;
        cur->state     = TASK_WAITING;
        /* Append to send_wait list. */
        cur->wait_next = NULL;
        if (queue->send_wait == NULL) {
            queue->send_wait = cur;
        } else {
            struct TaskControlBlock *tail = queue->send_wait;
            while (tail->wait_next) tail = tail->wait_next;
            tail->wait_next = cur;
        }
        if (timeout_ms != CREST_WAIT_FOREVER) {
            cur->delay_ticks        = timeout_ms;
            cur->blocking_wait_list = &queue->send_wait;
        } else {
            cur->delay_ticks        = 0;
            cur->blocking_wait_list = NULL;
        }
        timeout_ms = 0; /* one-shot: if we loop back and it's still full, return -1 */
        exit_critical(pm);
        port_trigger_pendsv();
        if (cur->flags & TASK_FLAG_TIMED_OUT) return -1;
        pm = enter_critical();
    }
}

/**
 * Pop an item. Returns 0 on success, -1 if the queue is empty or timed out.
 * The popped item is copied into the provided buffer.
 */
int z_impl_queue_pop(queue_t *queue, void *item, uint32_t timeout_ms)
{
    uint32_t pm = enter_critical();
    for (;;) {
        if (queue->head != queue->tail) {
            /* Data available — copy out and wake one sender. */
            memcpy(item,
                   (char *)queue->buffer +
                   (size_t)queue->head * queue->item_size, queue->item_size);
            queue->head = (queue->head + 1) % queue->slots;
            if (queue->send_wait != NULL) {
                struct TaskControlBlock *to_wake = queue->send_wait;
                queue->send_wait           = to_wake->wait_next;
                to_wake->wait_next         = NULL;
                to_wake->delay_ticks       = 0;
                to_wake->blocking_wait_list = NULL;
                exit_critical(pm);
                scheduler_wake_task(to_wake, 0);
                return 0;
            }
            exit_critical(pm);
            return 0;
        }
        /* Queue empty. */
        if (timeout_ms == 0) {
            exit_critical(pm);
            return -1;
        }
        struct TaskControlBlock *cur = scheduler_get_current();
        cur->flags    &= ~TASK_FLAG_TIMED_OUT;
        cur->state     = TASK_WAITING;
        /* Append to recv_wait list. */
        cur->wait_next = NULL;
        if (queue->recv_wait == NULL) {
            queue->recv_wait = cur;
        } else {
            struct TaskControlBlock *tail = queue->recv_wait;
            while (tail->wait_next) tail = tail->wait_next;
            tail->wait_next = cur;
        }
        if (timeout_ms != CREST_WAIT_FOREVER) {
            cur->delay_ticks        = timeout_ms;
            cur->blocking_wait_list = &queue->recv_wait;
        } else {
            cur->delay_ticks        = 0;
            cur->blocking_wait_list = NULL;
        }
        timeout_ms = 0; /* one-shot: if we loop back and it's still empty, return -1 */
        exit_critical(pm);
        port_trigger_pendsv();
        if (cur->flags & TASK_FLAG_TIMED_OUT) return -1;
        pm = enter_critical();
    }
}