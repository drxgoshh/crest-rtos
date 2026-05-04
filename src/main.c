#include <stdint.h>
#include "registers.h"
#include "task.h"
#include "port.h"
#include "uart.h"
#include "alloc.h"
#include "libc_stubs.h"
#include "mutex.h"
#include "queue.h"


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

mutex_t my_mutex; // Example mutex for demonstration
queue_t my_queue; // Example queue for demonstration


/* Simple blink tasks — they yield to let the scheduler run. */
static void idle_task(void *arg) {
    (void)arg;
    while (1) {
        __asm volatile ("wfi"); /* sleep until next interrupt */
    }
}

static void task1(void *arg) {
    (void)arg;
    while (1) {
        task_delay(500); /* simulate some work without blocking the scheduler */
    }
}

static void task2(void *arg) {
    (void)arg;
    while (1) {
        task_delay(500); /* simulate some work without blocking the scheduler */
    }
}

int main(void) {
    uart_init();    // Initialize UART for logging
    queue_init(&my_queue, sizeof(int), 10); // Initialize the queue before use
    crest_boot_banner(); // Print boot banner
    
    task_init(); // Initialize task subsystem
    uart_write("Creating tasks...\n");
    task_create(idle_task,   "Idle",   7, 256, NULL); /* lowest priority idle */
    task_create(task1, "Task1", 0, 512, NULL);
    task_create(task2, "Task2", 1, 512, NULL);
    uart_write("Scheduler started\n");
    port_start_first_task(); // Start the scheduler and run the first task


    /* Should never reach here */
    while (1) __asm volatile ("wfi");

    return 0;
}