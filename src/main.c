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

queue_t* my_queue; // Example queue for demonstration


/* Simple blink tasks — they yield to let the scheduler run. */
static void idle_task(void *arg) {
    (void)arg;
    while (1) {
        __asm volatile ("wfi"); /* sleep until next interrupt */
    }
}

static void task1(void *arg) {
    (void)arg;
    uart_write("Task1:\r\n");
    task_delay(500);
    /* task functions must never return — loop forever if done */
    while (1) task_delay(1000);
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
    task_create(task1, "Task1", 1, 512, NULL);
    task_create(task2, "Task2", 3, 512, NULL);
    uart_write("Scheduler started\n");

    port_start_first_task(); // Start the scheduler and run the first task


    /* Should never reach here */
    while (1) __asm volatile ("wfi");

    return 0;
}