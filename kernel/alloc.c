/* First-fit allocator for embedded use (static heap)
 * - single linked free list kept sorted by address
 * - block header stores total block size (including header)
 * - LSB of size == 1 => allocated, 0 => free
 * - split on allocation, coalesce on free
 * - protected by PRIMASK (disable interrupts) for simplicity
 */

#include "alloc.h"
#include "isr.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Heap comes from linker-provided symbols: __heap_start / __heap_end */
#define ALIGN 8
#define ALIGN_MASK (ALIGN - 1)
#define MIN_BLOCK_SIZE (sizeof(block_header_t) + ALIGN)

typedef struct block_header {
    size_t size; /* total size in bytes (includes header). LSB=1 => allocated */
    struct block_header *next; /* next free block (only valid when free) */
} block_header_t;

extern uint8_t __heap_start[];
extern uint8_t __heap_end[];
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
    uint8_t *start = (uint8_t *)__heap_start;
    uintptr_t aligned = ((uintptr_t)start + ALIGN_MASK) & ~((uintptr_t)ALIGN_MASK);
    uint8_t *heap_start = (uint8_t *)aligned;
    uint8_t *heap_end = (uint8_t *)__heap_end;
    if (heap_end <= heap_start) { heap_inited = true; free_list = NULL; return; }
    size_t total = (size_t)(heap_end - heap_start);
    free_list = (block_header_t *)heap_start;
    free_list->size = total;
    free_list->next = NULL;
    heap_inited = true;
}

void *malloc(size_t size)
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
            /* can allocate from this block */
            size_t remain = curr_sz - need;
            if (remain >= MIN_BLOCK_SIZE) {
                /* split: create a new free block after allocated part */
                block_header_t *next = (block_header_t *)((uint8_t *)curr + need);
                next->size = remain; /* free (LSB=0) */
                next->next = curr->next;

                /* link previous free to the new free block */
                if (prev) prev->next = next; else free_list = next;

                curr->size = need | (size_t)1; /* mark allocated */
            } else {
                /* use entire block (no split) */
                if (prev) prev->next = curr->next; else free_list = curr->next;
                curr->size = curr_sz | (size_t)1; /* mark allocated */
            }
            exit_critical(pm);
            return (void *)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }
    exit_critical(pm);
    return NULL; /* out of memory */
}

void free(void *ptr)
{
    if (!ptr) return;

    block_header_t *hdr = (block_header_t *)ptr - 1;
    uint8_t *hptr = (uint8_t *)hdr;
    /* compute heap bounds from linker-provided symbols (same alignment as heap_init) */
    uint8_t *start = (uint8_t *)__heap_start;
    uintptr_t aligned = ((uintptr_t)start + ALIGN_MASK) & ~((uintptr_t)ALIGN_MASK);
    uint8_t *heap_start = (uint8_t *)aligned;
    uint8_t *heap_end = (uint8_t *)__heap_end;

    /* basic pointer validation */
    if (hptr < heap_start || hptr >= heap_end) return;

    uint32_t pm = enter_critical();
    if (!heap_inited) { exit_critical(pm); return; }

    size_t bsz = hdr_size(hdr);
    if (bsz < sizeof(block_header_t) || hptr + bsz > heap_end) { exit_critical(pm); return; }

    /* mark free */
    hdr->size = bsz; /* clear allocated bit */

    /* insert into free list keeping list sorted by address */
    block_header_t *prev = NULL;
    block_header_t *curr = free_list;
    while (curr && curr < hdr) {
        prev = curr;
        curr = curr->next;
    }

    /* insert hdr between prev and curr */
    hdr->next = curr;
    if (prev) prev->next = hdr; else free_list = hdr;

    /* coalesce with next if adjacent */
    if (hdr->next) {
        uint8_t *hdr_end = (uint8_t *)hdr + hdr_size(hdr);
        if (hdr_end == (uint8_t *)hdr->next) {
            hdr->size = hdr_size(hdr) + hdr_size(hdr->next);
            hdr->next = hdr->next->next;
        }
    }

    /* coalesce with prev if adjacent */
    if (prev) {
        uint8_t *prev_end = (uint8_t *)prev + hdr_size(prev);
        if (prev_end == (uint8_t *)hdr) {
            prev->size = hdr_size(prev) + hdr_size(hdr);
            prev->next = hdr->next;
        }
    }

    exit_critical(pm);
}
