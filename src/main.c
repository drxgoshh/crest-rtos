#include <stdint.h>
#include "registers.h"
#include "task.h"
#include "port.h"
#include "uart.h"
#include "alloc.h"
#include "libc_stubs.h"
#include "mutex.h"
#include "queue.h"
#include "sched.h"
#include "kobject.h"
#include "semaphore.h"
#include "kwork.h"
#include <stdio.h>


void crest_boot_banner(void) {
    uart_write("\r\n");
    uart_write("  ██████╗██████╗ ███████╗███████╗████████╗\r\n");
    uart_write(" ██╔════╝██╔══██╗██╔════╝██╔════╝╚══██╔══╝\r\n");
    uart_write(" ██║     ██████╔╝█████╗  ███████╗   ██║   \r\n");
    uart_write(" ██║     ██╔══██╗██╔══╝  ╚════██║   ██║   \r\n");
    uart_write(" ╚██████╗██║  ██║███████╗███████║   ██║   \r\n");
    uart_write("  ╚═════╝╚═╝  ╚═╝╚══════╝╚══════╝   ╚═╝   \r\n");
    uart_write("\r\n");
    uart_write("  Compact Real-time Embedded SysTem\r\n");
    uart_write("  v0.1.0 | ARM Cortex-M4\r\n");
    uart_write("\r\n");
    
}

/* Shared kernel-object handles — initialised by task_producer before it
 * drops to unprivileged mode, read by task_consumer once non-zero. */
static volatile uint32_t g_qh   = KOBJ_INVALID;
static volatile uint32_t g_semh = KOBJ_INVALID;
static volatile uint32_t g_mh   = KOBJ_INVALID;

/*
 * g_consumer_stack_probe — pointer into the consumer task's stack region.
 * Published by task_consumer (while still privileged) so that task_mpu_probe
 * can attempt to access it from unprivileged mode after the consumer has
 * dropped privileges.  The MPU maps only the running task's OWN stack as RW;
 * any cross-task stack access must raise a MemManage fault.
 */
static volatile uint32_t *g_consumer_stack_probe = NULL;

/*
 * task_producer (priority 1)
 *
 * Creates the shared queue, semaphore, and mutex while still privileged,
 * then drops to user mode and loops:
 *   mutex_lock → format + push message → mutex_unlock → sem_give → delay
 *
 * This exercises: queue_push, mutex_lock/unlock, sem_give from user mode.
 */
static void task_producer(void *arg)
{
    (void)arg;
    uart_write("[producer] init (privileged)\r\n");

    /* Drop to unprivileged FIRST — creation syscalls now work from user mode. */
    uart_write("[producer] dropping to unprivileged\r\n");
    port_set_unprivileged();

    /* Create all primitives via SVC — kernel task allocates and returns handles. */
    g_qh = queue_create(8, 32);
    if (g_qh == KOBJ_INVALID) {
        k_uart_write("[producer] queue_create FAILED\r\n");
        for (;;) task_delay(1000);
    }

    g_semh = sem_create(0);
    if (g_semh == KOBJ_INVALID) {
        k_uart_write("[producer] sem_create FAILED\r\n");
        for (;;) task_delay(1000);
    }

    g_mh = mutex_create();
    if (g_mh == KOBJ_INVALID) {
        k_uart_write("[producer] mutex_create FAILED\r\n");
        for (;;) task_delay(1000);
    }

    k_uart_write("[producer] primitives created from user mode\r\n");

    /* All calls below go through SVC. */
    char msg[32];
    for (int i = 0; ; ++i) {
        mutex_lock(g_mh, CREST_WAIT_FOREVER);
        snprintf(msg, sizeof(msg), "item-%d", i);
        int r = queue_push(g_qh, msg, 0);
        mutex_unlock(g_mh);

        if (r == 0) {
            k_uart_write("[producer] pushed: ");
            k_uart_write(msg);
            k_uart_write("\r\n");
        } else {
            k_uart_write("[producer] queue full, dropping\r\n");
        }

        sem_give(g_semh); /* signal consumer that data is ready */
        task_delay(500);
    }
}

/*
 * task_consumer (priority 2)
 *
 * Waits for the shared handles to be published, drops to user mode, then
 * loops:
 *   sem_take (blocks until producer gives) → mutex_lock → pop → mutex_unlock
 *
 * This exercises: sem_take blocking, queue_pop, mutex contention.
 */
static void task_consumer(void *arg)
{
    (void)arg;
    uart_write("[consumer] waiting for handles\r\n");

    /* Spin (privileged) until the producer has published the handles. */
    while (g_qh == KOBJ_INVALID || g_semh == KOBJ_INVALID || g_mh == KOBJ_INVALID)
        task_delay(10);

    uart_write("[consumer] dropping to unprivileged\r\n");

    /* Publish a pointer into our own stack so task_mpu_probe can attempt
     * to access it from a different task's unprivileged context. */
    volatile uint32_t stack_canary = 0xCAFEBABEu;
    g_consumer_stack_probe = &stack_canary;

    port_set_unprivileged();

    char buf[32];
    for (;;) {
        (void)stack_canary; /* keep slot live so address stays valid */
        sem_take(g_semh, CREST_WAIT_FOREVER); /* block until producer signals */

        mutex_lock(g_mh, CREST_WAIT_FOREVER);
        int r = queue_pop(g_qh, buf, 0);
        mutex_unlock(g_mh);

        if (r == 0) {
            k_uart_write("[consumer] received: ");
            k_uart_write(buf);
            k_uart_write("\r\n");
        } else {
            k_uart_write("[consumer] queue empty after signal\r\n");
        }
    }
}

static void idle_task(void *arg)
{
    (void)arg;
    while (1)
        __asm volatile ("wfi");
}

/*
 * task_mpu_probe (priority 3)
 *
 * Waits until the consumer has published a pointer into its own stack, then
 * drops to unprivileged mode and deliberately writes to that address.
 *
 * Expected outcome:
 *   The MPU maps only the running task's own stack as RW.  The consumer's
 *   stack sits in the global RAM NO-ACCESS region from this task's
 *   perspective, so the store raises a MemManage fault immediately.
 *   MemManage_Handler prints the guilty task name and faulting address.
 */
static void task_mpu_probe(void *arg)
{
    (void)arg;
    uart_write("[mpu_probe] waiting for consumer stack address...\r\n");

    while (g_consumer_stack_probe == NULL)
        task_delay(50);

    uart_write("[mpu_probe] got consumer stack addr, dropping to unprivileged\r\n");
    port_set_unprivileged();

    /* Intentional MPU violation: write to the consumer's stack slot.
     * The MPU should raise MemManage before this store completes. */
    k_uart_write("[mpu_probe] attempting illegal write to consumer stack...\r\n");
    *g_consumer_stack_probe = 0xDEADBEEFu;  /* <<< MemManage expected here */

    /* Should never reach here if MPU is working correctly. */
    k_uart_write("[mpu_probe] ERROR: write was NOT caught by MPU!\r\n");
    while (1) task_delay(1000);
}

int main(void) {
    uart_init();    // Initialize UART for logging
    crest_boot_banner(); // Print boot banner
    task_init(); // Initialize task subsystem
    

    uart_write("Creating tasks...\n");

    task_create(kernel_task_fn,  "KernelWorker", 0, 512, NULL);
    task_create(task_producer,   "Producer",     1, 512, NULL);
    task_create(task_consumer,   "Consumer",     2, 512, NULL);
    task_create(task_mpu_probe,  "MpuProbe",     3, 512, NULL);
    task_create(idle_task,       "Idle",         7, 512, NULL);
    
    
    
    uart_write("Scheduler started\n");

    port_start_first_task(); // Start the scheduler and run the first task


    /* Should never reach here */
    while (1) __asm volatile ("wfi");

    return 0;
}