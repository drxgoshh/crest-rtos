#ifndef CREST_LIBC_STUBS_H
#define CREST_LIBC_STUBS_H

#include <stddef.h>
#include <stdarg.h>

/* Minimal libc stubs provided by kernel/libc_stubs.c */
void *memset(void *s, int c, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
int   vsnprintf(char *str, size_t size, const char *format, va_list ap);
int   snprintf(char *str, size_t size, const char *format, ...);

#endif /* CREST_LIBC_STUBS_H */
