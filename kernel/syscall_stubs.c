/* syscall_stubs.c
 * User-facing kernel API wrappers. Each stub checks privilege at runtime:
 *   privileged   → call z_impl_* directly
 *   unprivileged → trap via SVC; handler validates handle + dispatches
 */
#include <stdint.h>
#include "port.h"
#include "syscall.h"
#include "kobject.h"
#include "task.h"
#include "queue.h"
#include "semaphore.h"
#include "mutex.h"

/* ── kernel implementations ─────────────────────────────────────────────── */
extern void      z_impl_task_delay(uint32_t ticks);
extern void      uart_write(const char *s);
extern queue_t  *z_impl_queue_create(uint32_t item_size, uint32_t size);
extern int       z_impl_queue_push(queue_t *q, const void *item, uint32_t ms);
extern int       z_impl_queue_pop(queue_t *q, void *item, uint32_t ms);
extern mutex_t  *z_impl_mutex_create(void);
extern void      z_impl_mutex_lock(mutex_t *m);
extern void      z_impl_mutex_unlock(mutex_t *m);
extern semaphore_t *z_impl_sem_create(int initial_count);
extern void      z_impl_sem_take(semaphore_t *s);
extern void      z_impl_sem_give(semaphore_t *s);

/* ── k_uart_write — safe from unprivileged mode ─────────────────────────── */
void k_uart_write(const char *s)
{
    if (port_is_privileged())
        uart_write(s);
    else
        (void)port_syscall_invoke(SYSCALL_UART_WRITE,
                                  (unsigned int)(uintptr_t)s, 0, 0, 0);
}

/* ── task_delay ──────────────────────────────────────────────────────────── */
void task_delay(uint32_t ticks)
{
    if (port_is_privileged())
        z_impl_task_delay(ticks);
    else
        (void)port_syscall_invoke(SYSCALL_DELAY, ticks, 0, 0, 0);
}

/* ── queue ───────────────────────────────────────────────────────────────── */
uint32_t queue_create(uint32_t item_size, uint32_t size)
{
    queue_t *q = z_impl_queue_create(item_size, size);
    if (!q) return KOBJ_INVALID;
    return k_obj_alloc(q, KOBJ_QUEUE);
}

int queue_push(uint32_t handle, const void *item, uint32_t timeout_ms)
{
    if (port_is_privileged()) {
        queue_t *q = k_obj_get(handle, KOBJ_QUEUE);
        if (!q) return -1;
        return z_impl_queue_push(q, item, timeout_ms);
    }
    return (int)port_syscall_invoke(SYSCALL_QUEUE_SEND,
                                    (unsigned int)handle,
                                    (unsigned int)(uintptr_t)item,
                                    timeout_ms, 0);
}

int queue_pop(uint32_t handle, void *item, uint32_t timeout_ms)
{
    if (port_is_privileged()) {
        queue_t *q = k_obj_get(handle, KOBJ_QUEUE);
        if (!q) return -1;
        return z_impl_queue_pop(q, item, timeout_ms);
    }
    return (int)port_syscall_invoke(SYSCALL_QUEUE_RECV,
                                    (unsigned int)handle,
                                    (unsigned int)(uintptr_t)item,
                                    timeout_ms, 0);
}

/* ── semaphore ───────────────────────────────────────────────────────────── */
uint32_t sem_create(int initial_count)
{
    semaphore_t *s = z_impl_sem_create(initial_count);
    if (!s) return KOBJ_INVALID;
    return k_obj_alloc(s, KOBJ_SEM);
}

void sem_give(uint32_t handle)
{
    if (port_is_privileged()) {
        semaphore_t *s = k_obj_get(handle, KOBJ_SEM);
        if (s) z_impl_sem_give(s);
    } else {
        (void)port_syscall_invoke(SYSCALL_SEM_POST,
                                  (unsigned int)handle, 0, 0, 0);
    }
}

void sem_take(uint32_t handle)
{
    if (port_is_privileged()) {
        semaphore_t *s = k_obj_get(handle, KOBJ_SEM);
        if (s) z_impl_sem_take(s);
    } else {
        (void)port_syscall_invoke(SYSCALL_SEM_WAIT,
                                  (unsigned int)handle, 0, 0, 0);
    }
}

/* ── mutex ───────────────────────────────────────────────────────────────── */
uint32_t mutex_create(void)
{
    mutex_t *m = z_impl_mutex_create();
    if (!m) return KOBJ_INVALID;
    return k_obj_alloc(m, KOBJ_MUTEX);
}

void mutex_lock(uint32_t handle)
{
    if (port_is_privileged()) {
        mutex_t *m = k_obj_get(handle, KOBJ_MUTEX);
        if (m) z_impl_mutex_lock(m);
    } else {
        (void)port_syscall_invoke(SYSCALL_MUTEX_LOCK,
                                  (unsigned int)handle, 0, 0, 0);
    }
}

void mutex_unlock(uint32_t handle)
{
    if (port_is_privileged()) {
        mutex_t *m = k_obj_get(handle, KOBJ_MUTEX);
        if (m) z_impl_mutex_unlock(m);
    } else {
        (void)port_syscall_invoke(SYSCALL_MUTEX_UNLOCK,
                                  (unsigned int)handle, 0, 0, 0);
    }
}
