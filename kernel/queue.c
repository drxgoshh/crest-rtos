#include "task.h"
#include "port.h"
#include "queue.h"
#include "alloc.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

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

    mutex_init(&queue->mutex);
    return 0; /* Success */
}

/**
 * Push an item. Returns 0 on success, -1 if the queue is full.
 * 
 */
int queue_push(queue_t *queue, const void *item){
    mutex_lock(&queue->mutex);
    uint32_t next_index = (queue->tail + 1) % queue->slots;
    if (next_index == queue->head) {
        mutex_unlock(&queue->mutex);
        return -1; // Queue is full
    }
    memcpy((char*)queue->buffer + ((size_t)queue->tail * queue->item_size), item, queue->item_size);
    queue->tail = next_index;
    mutex_unlock(&queue->mutex);
    return 0; // Success
}

/**
 * Pop an item. Returns 0 on success, -1 if the queue is empty.
 * The popped item is copied into the provided buffer.
 * 
 */
int queue_pop(queue_t *queue, void *item){
    mutex_lock(&queue->mutex);
    if (queue->head == queue->tail) {
        mutex_unlock(&queue->mutex);
        return -1; // Queue is empty
    }
    memcpy(item, (char*)queue->buffer + ((size_t)queue->head * queue->item_size), queue->item_size);
    queue->head = (queue->head + 1) % queue->slots;
    mutex_unlock(&queue->mutex);
    return 0; // Success
}
