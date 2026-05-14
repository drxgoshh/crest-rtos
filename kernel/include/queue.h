#ifndef CREST_QUEUE_H
#define CREST_QUEUE_H

#include <stdint.h>

/* Opaque queue type. Use `queue_create` to allocate a new queue. */
typedef struct queue queue_t;

/* Create a queue. Returns a kernel object handle (KOBJ_INVALID on failure). */
uint32_t queue_create(uint32_t item_size, uint32_t size);

/* Push an item into the queue. Blocks up to `timeout_ms` milliseconds.
 * Returns 0 on success, -1 on failure or timeout.
 */
int queue_push(uint32_t handle, const void *item, uint32_t timeout_ms);

/* Pop an item from the queue into `item`. Blocks up to `timeout_ms`.
 * Returns 0 on success, -1 on failure or timeout.
 */
int queue_pop(uint32_t handle, void *item, uint32_t timeout_ms);

/* Called from the system tick to age queue wait lists (internal). */
void queue_tick_all(void);

#endif /* CREST_QUEUE_H */
