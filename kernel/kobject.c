/* kobject.c — kernel object handle table
 *
 * Static table of KOBJ_MAX slots.  Each slot stores:
 *   ptr    — real kernel object pointer
 *   type   — kobj_type_t (KOBJ_QUEUE / KOBJ_SEM / KOBJ_MUTEX)
 *   in_use — 1 if allocated, 0 if free
 *
 * Handles are indices into this table (0 .. KOBJ_MAX-1).
 * Invalid handle = -1.
 */
#include "kobject.h"
#include "isr.h"
#include <stddef.h>
#include <stdint.h>

extern void uart_write(const char *s); /* for error reporting from k_obj_get() */

typedef struct {
    void        *ptr;
    kobj_type_t  type;
    uint8_t      in_use;
    uint16_t     gen;    /* generation counter for ABA protection */
} kobj_entry_t;

static kobj_entry_t table[KOBJ_MAX];

/* Pack handle: high 16 bits = generation, low 16 bits = index */
static inline kobj_handle_t pack_handle(uint16_t gen, uint16_t idx)
{
    return ((kobj_handle_t)gen << 16) | (kobj_handle_t)idx;
}

/* Unpack index/gen */
static inline uint16_t handle_index(kobj_handle_t h) { return (uint16_t)(h & 0xFFFFu); }
static inline uint16_t handle_gen(kobj_handle_t h)   { return (uint16_t)(h >> 16); }

/* Find a free slot, initialize generation and return a handle. */
kobj_handle_t k_obj_alloc(void *ptr, kobj_type_t type)
{
    if (!ptr) return KOBJ_INVALID;
    uint32_t pm = enter_critical();
    for (uint16_t i = 0; i < KOBJ_MAX; i++) {
        if (!table[i].in_use) {
            /* advance generation to ensure handle uniqueness */
            table[i].gen = (uint16_t)(table[i].gen + 1);
            if (table[i].gen == 0) table[i].gen = 1;
            table[i].ptr = ptr;
            table[i].type = type;
            table[i].in_use = 1;
            
            kobj_handle_t h = pack_handle(table[i].gen, i);
            exit_critical(pm);
            return h;
        }
    }
    exit_critical(pm);
    return KOBJ_INVALID;
}

/* Validate handle and return pointer if type matches. */
void *k_obj_get(kobj_handle_t handle, kobj_type_t type)
{
    if (handle == KOBJ_INVALID) return NULL;
    uint16_t idx = handle_index(handle);
    uint16_t gen = handle_gen(handle);
    if (idx >= KOBJ_MAX) return NULL;
    uint32_t pm = enter_critical();
    if (!table[idx].in_use) { exit_critical(pm); return NULL; }
    if (table[idx].gen != gen) { exit_critical(pm); return NULL; }
    if (table[idx].type != type) { exit_critical(pm); return NULL; }
    void *ptr = table[idx].ptr;
    exit_critical(pm);
    return ptr;
}

void k_obj_free(kobj_handle_t handle)
{
    if (handle == KOBJ_INVALID) return;
    uint16_t idx = handle_index(handle);
    uint16_t gen = handle_gen(handle);
    if (idx >= KOBJ_MAX) return;
    uint32_t pm = enter_critical();
    if (table[idx].in_use && table[idx].gen == gen) {
        table[idx].in_use = 0;
        table[idx].ptr = NULL;
        table[idx].type = (kobj_type_t)0;
        /* bump generation to avoid immediate reuse matching old handles */
        table[idx].gen = (uint16_t)(table[idx].gen + 1);
        if (table[idx].gen == 0) table[idx].gen = 1;
    }
    exit_critical(pm);
}
