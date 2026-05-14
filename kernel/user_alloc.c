/* Simple first-fit allocator for the user heap region. Mirrors kernel allocator
 * but operates on linker-provided __user_heap_start / __user_heap_end symbols.
 */

#include "user_alloc.h"
#include "isr.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Heap alignment and header definitions follow kernel/alloc.c conventions */
#define ALIGN 8
#define ALIGN_MASK (ALIGN - 1)
#define MIN_BLOCK_SIZE (sizeof(block_header_t) + ALIGN)

typedef struct block_header {
    size_t size; /* total size in bytes (includes header). LSB=1 => allocated */
    struct block_header *next; /* next free block (only valid when free) */
} block_header_t;

extern uint8_t __user_heap_start[];
extern uint8_t __user_heap_end[];
static block_header_t *free_list = NULL;
static bool heap_inited = false;

static inline size_t align_up(size_t x)
{
    return (x + ALIGN_MASK) & ~(size_t)ALIGN_MASK;
}

static inline size_t hdr_size(const block_header_t *h)
{
    return h->size & ~(size_t)1;
}

static inline bool hdr_allocated(const block_header_t *h)
{
    return (h->size & 1) != 0;
}

static void heap_init(void)
{
    uint8_t *start = (uint8_t *)__user_heap_start;
    uintptr_t aligned = ((uintptr_t)start + ALIGN_MASK) & ~((uintptr_t)ALIGN_MASK);
    uint8_t *heap_start = (uint8_t *)aligned;
    uint8_t *heap_end = (uint8_t *)__user_heap_end;
    if (heap_end <= heap_start) { heap_inited = true; free_list = NULL; return; }
    size_t total = (size_t)(heap_end - heap_start);
    free_list = (block_header_t *)heap_start;
    free_list->size = total;
    free_list->next = NULL;
    heap_inited = true;
}

void *user_malloc(size_t size)
{
    if (size == 0) return NULL;

    size = align_up(size);
    size_t need = size + sizeof(block_header_t);
    if (need < MIN_BLOCK_SIZE) need = MIN_BLOCK_SIZE;

    uint32_t pm = enter_critical();
    if (!heap_inited) heap_init();

    block_header_t *prev = NULL;
    block_header_t *curr = free_list;
    while (curr) {
        size_t curr_sz = hdr_size(curr);
        if (curr_sz >= need) {
            size_t remain = curr_sz - need;
            if (remain >= MIN_BLOCK_SIZE) {
                block_header_t *next = (block_header_t *)((uint8_t *)curr + need);
                next->size = remain;
                next->next = curr->next;
                if (prev) prev->next = next; else free_list = next;
                curr->size = need | (size_t)1;
            } else {
                if (prev) prev->next = curr->next; else free_list = curr->next;
                curr->size = curr_sz | (size_t)1;
            }
            exit_critical(pm);
            return (void *)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }
    exit_critical(pm);
    return NULL;
}

void user_free(void *ptr)
{
    if (!ptr) return;

    block_header_t *hdr = (block_header_t *)ptr - 1;
    uint8_t *hptr = (uint8_t *)hdr;
    uint8_t *start = (uint8_t *)__user_heap_start;
    uintptr_t aligned = ((uintptr_t)start + ALIGN_MASK) & ~((uintptr_t)ALIGN_MASK);
    uint8_t *heap_start = (uint8_t *)aligned;
    uint8_t *heap_end = (uint8_t *)__user_heap_end;
    if (hptr < heap_start || hptr >= heap_end) return;

    uint32_t pm = enter_critical();
    if (!heap_inited) { exit_critical(pm); return; }

    size_t bsz = hdr_size(hdr);
    if (bsz < sizeof(block_header_t) || hptr + bsz > heap_end) { exit_critical(pm); return; }

    hdr->size = bsz; /* clear allocated bit */

    block_header_t *prev = NULL;
    block_header_t *curr = free_list;
    while (curr && curr < hdr) { prev = curr; curr = curr->next; }

    hdr->next = curr;
    if (prev) prev->next = hdr; else free_list = hdr;

    if (hdr->next) {
        uint8_t *hdr_end = (uint8_t *)hdr + hdr_size(hdr);
        if (hdr_end == (uint8_t *)hdr->next) {
            hdr->size = hdr_size(hdr) + hdr_size(hdr->next);
            hdr->next = hdr->next->next;
        }
    }
    if (prev) {
        uint8_t *prev_end = (uint8_t *)prev + hdr_size(prev);
        if (prev_end == (uint8_t *)hdr) {
            prev->size = hdr_size(prev) + hdr_size(hdr);
            prev->next = hdr->next;
        }
    }

    exit_critical(pm);
}
