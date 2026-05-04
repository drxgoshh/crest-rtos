#ifndef CREST_QUEUE_H
#define CREST_QUEUE_H

#include <stdint.h>
#include "task.h"
#include "mutex.h"

typedef struct {
    uint32_t item_size;    /* size of each item in bytes */
    uint32_t size;         /* requested maximum number of items the queue can hold (capacity) */
    uint32_t slots;        /* internal buffer slots (size + 1) used for ring buffer arithmetic */
    uint32_t head;         /* index of the head of the queue */
    uint32_t tail;         /* index of the tail of the queue */
    void *buffer;          /* pointer to the queue's data buffer */
    mutex_t mutex;         /* mutex to protect access to the queue */
} queue_t;

/**
 * Queue initialization. Must be called before using the queue.
 * 
 */
int queue_init(queue_t *queue, uint32_t item_size, uint32_t size);

/**
 * Push an item. Returns 0 on success, -1 if the queue is full.
 * 
 */
int queue_push(queue_t *queue, const void *item);

/**
 * Pop an item. Returns 0 on success, -1 if the queue is empty.
 * The popped item is copied into the provided buffer.
 * 
 */
int queue_pop(queue_t *queue, void *item);


#endif /* CREST_QUEUE_H */
