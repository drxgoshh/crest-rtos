#ifndef CREST_ALLOC_H
#define CREST_ALLOC_H

#include <stddef.h>

/* Simple embedded allocator API */
void *malloc(size_t size);
void free(void *ptr);

#endif /* CREST_ALLOC_H */
