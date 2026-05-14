#ifndef CREST_QUEUE_H
#define CREST_QUEUE_H

#include <stdint.h>

/* Opaque queue type. Use `queue_create` to allocate a new queue. */
typedef struct queue queue_t;

/* Create a queue with items of `item_size` bytes and capacity `size`.
 * Returns pointer to new queue or NULL on failure.
 */
queue_t *queue_create(uint32_t item_size, uint32_t size);

/* Push an item into the queue. Blocks up to `timeout_ms` milliseconds.
 * Returns 0 on success, -1 on failure or timeout.
 */
int queue_push(queue_t *queue, const void *item, uint32_t timeout_ms);

/* Pop an item from the queue into `item`. Blocks up to `timeout_ms`.
 * Returns 0 on success, -1 on failure or timeout.
 */
int queue_pop(queue_t *queue, void *item, uint32_t timeout_ms);

/* Called from the system tick to age queue wait lists (internal). */
void queue_tick_all(void);

#endif /* CREST_QUEUE_H */
