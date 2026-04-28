#ifndef CREST_TASK_H
#define CREST_TASK_H

#include <stdint.h>

#define TASK_NAME_MAX_LEN 16
#define MAX_TASK_PRIORITIES 8
#define MAX_TASKS 8
#define MAX_TASK_STACK_SIZE 1024

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_WAITING,   /* sleeping via task_delay() */
    TASK_SUSPENDED
} task_state_t;

/* Task Control Block (TCB) */
struct TaskControlBlock {
    uint8_t *stack_base;                   /* pointer to stack memory (heap) */
    uint32_t stack_size;                   /* size of allocated stack */
    char name[TASK_NAME_MAX_LEN];          /* task name (NUL terminated) */
    uint32_t *stack_pointer;               /* current stack pointer (for context switch) */
    task_state_t state;                    /* task state */
    uint32_t priority;                     /* lower = higher priority */
    struct TaskControlBlock *next;         /* singly-linked circular ready list */
    uint32_t delay_ticks;                  /* ticks remaining when blocked */
    void (*task_function)(void*);          /* entry function */
    void* arg;
};

/* Public API (minimal) */
void task_init(void);
void task_create(void (*task_function)(void*), const char *name, uint32_t priority, uint32_t stack_size, void* arg);
void task_delete(struct TaskControlBlock *tcb);
void task_yield(void);
void task_delay(uint32_t ticks);

/* Scheduler helpers used by the port layer */
struct TaskControlBlock *scheduler_get_current(void);
void scheduler_set_current(struct TaskControlBlock *tcb);
struct TaskControlBlock *scheduler_get_next(void);
void scheduler_tick(void);

#endif /* CREST_TASK_H */