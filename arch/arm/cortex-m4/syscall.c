/* syscall.c — SVC handler and dispatch for Cortex-M4
 *
 * Exception frame (pushed by hardware on SVC entry, readable via PSP/MSP):
 *   frame[0] R0  — arg0 / return value
 *   frame[1] R1  — arg1
 *   frame[2] R2  — arg2
 *   frame[3] R3  — arg3
 *   frame[4] R12 — syscall id (set by port_syscall_invoke before SVC)
 *   frame[5] LR
 *   frame[6] PC
 *   frame[7] xPSR
 *
 * EXC_RETURN bit 2: 0 = MSP, 1 = PSP.
 */
#include <stdint.h>
#include "syscall.h"
#include "port.h"
#include "kobject.h"
#include "queue.h"
#include "semaphore.h"
#include "mutex.h"

extern void z_impl_task_delay(uint32_t ticks);
extern void uart_write(const char *s);
extern int  z_impl_queue_push(queue_t *q, const void *item, uint32_t ms);
extern int  z_impl_queue_pop(queue_t *q, void *item, uint32_t ms);
extern void z_impl_sem_give(semaphore_t *s);
extern void z_impl_sem_take(semaphore_t *s);
extern void z_impl_mutex_lock(mutex_t *m);
extern void z_impl_mutex_unlock(mutex_t *m);

/* frame[4] = stacked R12 = syscall id */
void svc_handler_c(uint32_t *frame);

/* YOUR TURN: implement the naked SVC_Handler below.
 * Requirements:
 *   1. Determine whether PSP or MSP holds the stacked frame (check LR bit 2).
 *   2. Load the frame pointer into R0.
 *   3. Load the syscall id (currently in R7) into R1.
 *   4. Branch (not BL) to svc_handler_c.
 */
__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "push {lr}\n"
        "bl svc_handler_c\n"
        "pop {lr}\n"
        "bx lr\n"
    );
}

/* YOUR TURN: fill in the dispatch switch.
 * frame[0] is arg0; write the return value back into frame[0].
 */
void svc_handler_c(uint32_t *frame)
{
    uint32_t id = frame[4];
    uint32_t retval = 0;

    switch ((syscall_id_t)id) {
    case SYSCALL_DELAY:
        z_impl_task_delay(frame[0]);
        retval = 0;
        break;

    case SYSCALL_QUEUE_SEND:
        {
            kobj_handle_t h = (kobj_handle_t)frame[0];
            void *item = (void *)(uintptr_t)frame[1];
            uint32_t timeout = frame[2];
            queue_t *q = (queue_t *)k_obj_get(h, KOBJ_QUEUE);
            if (!q) {
                retval = (uint32_t)-1;
            } else {
                retval = (uint32_t)z_impl_queue_push(q, item, timeout);
            }
        }
        break;

    case SYSCALL_QUEUE_RECV:
        {
            kobj_handle_t h = (kobj_handle_t)frame[0];
            void *item = (void *)(uintptr_t)frame[1];
            uint32_t timeout = frame[2];
            queue_t *q = (queue_t *)k_obj_get(h, KOBJ_QUEUE);
            if (!q) {
                retval = (uint32_t)-1;
            } else {
                retval = (uint32_t)z_impl_queue_pop(q, item, timeout);
            }
        }
        break;

    case SYSCALL_SEM_POST:
        {
            kobj_handle_t h = (kobj_handle_t)frame[0];
            semaphore_t *s = (semaphore_t *)k_obj_get(h, KOBJ_SEM);
            if (!s) {
                retval = (uint32_t)-1;
            } else {
                z_impl_sem_give(s);
                retval = 0;
            }
        }
        break;

    case SYSCALL_SEM_WAIT:
        {
            kobj_handle_t h = (kobj_handle_t)frame[0];
            semaphore_t *s = (semaphore_t *)k_obj_get(h, KOBJ_SEM);
            if (!s) {
                retval = (uint32_t)-1;
            } else {
                z_impl_sem_take(s);
                retval = 0;
            }
        }
        break;

    case SYSCALL_MUTEX_LOCK:
        {
            kobj_handle_t h = (kobj_handle_t)frame[0];
            mutex_t *m = (mutex_t *)k_obj_get(h, KOBJ_MUTEX);
            if (!m) {
                retval = (uint32_t)-1;
            } else {
                z_impl_mutex_lock(m);
                retval = 0;
            }
        }
        break;

    case SYSCALL_MUTEX_UNLOCK:
        {
            kobj_handle_t h = (kobj_handle_t)frame[0];
            mutex_t *m = (mutex_t *)k_obj_get(h, KOBJ_MUTEX);
            if (!m) {
                retval = (uint32_t)-1;
            } else {
                z_impl_mutex_unlock(m);
                retval = 0;
            }
        }
        break;

    case SYSCALL_UART_WRITE:
        uart_write((const char *)(uintptr_t)frame[0]);
        retval = 0;
        break;

    default:
        uart_write("[CREST] SVC: unknown syscall id\r\n");
        retval = (uint32_t)-1;
        break;
    }

    frame[0] = retval;
}
