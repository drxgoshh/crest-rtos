/* user_alloc.h — simple allocator for user-mode heap region */
#ifndef CREST_USER_ALLOC_H
#define CREST_USER_ALLOC_H

#include <stddef.h>

void *user_malloc(size_t size);
void  user_free(void *ptr);

#endif /* CREST_USER_ALLOC_H */
