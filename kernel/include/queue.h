#ifndef CREST_QUEUE_H
#define CREST_QUEUE_H

#include <stdint.h>
#include "task.h"

typedef struct queue queue_t;


/**
 * Create a queue. Allocates memory for the queue's buffer.
 * Returns 0 on success, -1 on failure.
 */
queue_t* queue_create(uint32_t item_size, uint32_t size);

/**
 * Queue initialization. Must be called before using the queue.
 */
int queue_init(queue_t *queue, uint32_t item_size, uint32_t size);

/**
 * Push an item. Returns 0 on success, -1 if the queue is full.
 * 
 */
int queue_push(queue_t *queue, const void *item, uint32_t timeout_ms);

/**
 * Pop an item. Returns 0 on success, -1 if the queue is empty.
 * The popped item is copied into the provided buffer.
 * 
 */
int queue_pop(queue_t *queue, void *item, uint32_t timeout_ms);

void queue_tick_all(void);

#endif /* CREST_QUEUE_H */
