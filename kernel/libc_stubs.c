#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n && (*d++ = *src++)) --n;
    while (n--) *d++ = '\0';
    return dest;
}

/* Minimal, safe vsnprintf/snprintf for embedded use.
 * Supports: %s, %c, %d, %i, %u, %x, %X, %p, %%
 * Returns number of characters that would have been written (excluding NUL).
 */

static inline void emit_char(char c, char **dst, size_t *avail, int *total)
{
    if (*avail > 0) {
        **dst = c;
        (*dst)++;
        (*avail)--;
    }
    (*total)++;
}

static void emit_string(const char *s, char **dst, size_t *avail, int *total)
{
    if (!s) s = "(null)";
    while (*s) emit_char(*s++, dst, avail, total);
}

static void emit_unsigned(unsigned long val, int base, const char *digits,
                          char **dst, size_t *avail, int *total)
{
    char buf[32];
    int idx = 0;
    if (val == 0) buf[idx++] = '0';
    else {
        while (val && idx < (int)sizeof(buf)) {
            buf[idx++] = digits[val % base];
            val /= base;
        }
    }
    for (int i = idx - 1; i >= 0; --i) emit_char(buf[i], dst, avail, total);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    char *dst = str;
    size_t avail = (size > 0) ? (size - 1) : 0; /* reserve NUL */
    int total = 0;

    while (*format) {
        if (*format != '%') { emit_char(*format++, &dst, &avail, &total); continue; }
        format++; /* skip '%' */
        if (*format == '%') { emit_char('%', &dst, &avail, &total); format++; continue; }

        /* No full flag/width parsing here - keep it small */
        uint8_t long_mod = 0;
        if (*format == 'l') { long_mod = 1; format++; }

        char spec = *format++;
        switch (spec) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                emit_string(s, &dst, &avail, &total);
                break;
            }
            case 'c': {
                int c = va_arg(ap, int);
                emit_char((char)c, &dst, &avail, &total);
                break;
            }
            case 'd': case 'i': {
                long v = long_mod ? va_arg(ap, long) : va_arg(ap, int);
                if (v < 0) { emit_char('-', &dst, &avail, &total); v = -v; }
                emit_unsigned((unsigned long)v, 10, "0123456789", &dst, &avail, &total);
                break;
            }
            case 'u': {
                unsigned long v = long_mod ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                emit_unsigned(v, 10, "0123456789", &dst, &avail, &total);
                break;
            }
            case 'x': {
                unsigned long v = long_mod ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                emit_unsigned(v, 16, "0123456789abcdef", &dst, &avail, &total);
                break;
            }
            case 'X': {
                unsigned long v = long_mod ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                emit_unsigned(v, 16, "0123456789ABCDEF", &dst, &avail, &total);
                break;
            }
            case 'p': {
                void *p = va_arg(ap, void *);
                uintptr_t v = (uintptr_t)p;
                emit_char('0', &dst, &avail, &total);
                emit_char('x', &dst, &avail, &total);
                emit_unsigned((unsigned long)v, 16, "0123456789abcdef", &dst, &avail, &total);
                break;
            }
            default:
                /* Unknown specifier - emit it literally */
                emit_char('%', &dst, &avail, &total);
                emit_char(spec, &dst, &avail, &total);
                break;
        }
    }

    if (size > 0) *dst = '\0';
    return total;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int r = vsnprintf(str, size, format, ap);
    va_end(ap);
    return r;
}

