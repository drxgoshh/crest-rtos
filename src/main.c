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

/* Test handles (created while privileged, exercised after dropping to user mode) */
// queue_t* my_queue; // no longer needed

static void user_test(void *arg) {
    (void)arg;
    uart_write("user_test: setup (privileged)\r\n");

    uint32_t qh = queue_create(32, 4);
    if (qh == KOBJ_INVALID) {
        uart_write("user_test: queue_create failed\r\n");
        for (;;) task_delay(1000);
    }

    uint32_t semh = sem_create(0);
    if (semh == KOBJ_INVALID) {
        uart_write("user_test: sem_create failed\r\n");
        for (;;) task_delay(1000);
    }

    uint32_t mh = mutex_create();
    if (mh == KOBJ_INVALID) {
        uart_write("user_test: mutex_create failed\r\n");
        for (;;) task_delay(1000);
    }

    uart_write("user_test: dropping to unprivileged mode\r\n");
    port_set_unprivileged(); /* now syscalls go through SVC */

    /* ── all calls below go through SVC (unprivileged) ── */

    /* exercise queue: push then pop */
    char msg[32];
    for (int i = 0; i < 3; ++i) {
        snprintf(msg, sizeof(msg), "msg-%d", i);
        k_uart_write("user_test: queue_push\r\n");
        int r = queue_push(qh, msg, 100);
        // snprintf(msg, sizeof(msg), "user_test: qh: %u\r\n", qh);
        if (r == 0) k_uart_write("user_test: push OK\r\n"); else k_uart_write("user_test: push FAIL\r\n");
        snprintf(msg, sizeof(msg), "returned %d\r\n", r);
        k_uart_write(msg);
        task_delay(100);
    }

    char buf[32];
    for (int i = 0; i < 3; ++i) {
        k_uart_write("user_test: queue_pop\r\n");
        int r = queue_pop(qh, buf, 100);
        if (r == 0) {
            k_uart_write("user_test: pop OK: ");
            k_uart_write(buf);
            k_uart_write("\r\n");
        } else {
            k_uart_write("user_test: pop FAIL\r\n");
        }
        task_delay(100);
    }

    /* semaphore and mutex */
    k_uart_write("user_test: sem_give\r\n");
    sem_give(semh);
    k_uart_write("user_test: sem_take\r\n");
    sem_take(semh);

    k_uart_write("user_test: mutex lock/unlock\r\n");
    mutex_lock(mh);
    mutex_unlock(mh);

    k_uart_write("user_test: finished\r\n");
    while (1) task_delay(1000);
}


/* Simple blink tasks — they yield to let the scheduler run. */
static void idle_task(void *arg) {
    (void)arg;
    while (1) {
        __asm volatile ("wfi"); /* sleep until next interrupt */
    }
}

static void task1(void *arg) {
    (void)arg;
    while (1){
        uart_write("Task1:\r\n");
        task_delay(1000);
    }
}

static void task2(void *arg) {
    (void)arg;
    while (1) {
        uart_write("Task2\r\n");
        task_delay(500);
    } 
}

int main(void) {
    uart_init();    // Initialize UART for logging
    crest_boot_banner(); // Print boot banner
    
    task_init(); // Initialize task subsystem
    uart_write("Creating tasks...\n");
    task_create(idle_task,   "Idle",   7, 512, NULL); /* lowest priority idle */
    task_create(user_test, "UserTest", 1, 512, NULL);
    uart_write("Scheduler started\n");

    port_start_first_task(); // Start the scheduler and run the first task


    /* Should never reach here */
    while (1) __asm volatile ("wfi");

    return 0;
}