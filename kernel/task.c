#include "task.h"
#include "sched.h"
#include "port.h"
#include "alloc.h"
#include "isr.h"
#include "../boards/stm32f446/uart.h"
#include <string.h>
#include <stdint.h>

extern uint8_t g_priority_mask; /* bitmask of priorities with ready tasks */

/* ------------------------------------------------------------------ */
/* TCB pool — fixed-size static allocation                             */
/* ------------------------------------------------------------------ */
static struct TaskControlBlock tcb_pool[MAX_TASKS];
static uint8_t                 tcb_used[MAX_TASKS];

static struct TaskControlBlock *get_free_tcb(void)
{
    uint32_t pm = enter_critical();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!tcb_used[i]) {
            tcb_used[i] = 1;
            struct TaskControlBlock *tcb = &tcb_pool[i];
            exit_critical(pm);
            return tcb;
        }
    }
    exit_critical(pm);
    return NULL;
}

static void free_tcb(struct TaskControlBlock *tcb)
{
    uint32_t pm = enter_critical();
    if (tcb) {
        int idx = (int)(tcb - tcb_pool);
        if (idx >= 0 && idx < MAX_TASKS) tcb_used[idx] = 0;
    }
    exit_critical(pm);
}

/* ------------------------------------------------------------------ */
/* Public init                                                          */
/* ------------------------------------------------------------------ */

void task_init(void)
{
    memset(tcb_pool, 0, sizeof(tcb_pool));
    memset(tcb_used, 0, sizeof(tcb_used));
    scheduler_init();
}

/* ------------------------------------------------------------------ */
/* Stack frame initialisation                                           */
/*                                                                      */
/* Lays out the fake exception frame so the context-switch handler can  */
/* restore it as if the task had been preempted after one instruction.  */
/* ------------------------------------------------------------------ */
static uint32_t *stack_init(uint8_t *stack_base, uint32_t stack_size,
                             void (*task_fn)(void *), void *arg)
{
    /* Align top of stack to 8 bytes (Cortex-M ABI requirement) */
    uintptr_t top = ((uintptr_t)stack_base + stack_size) & ~(uintptr_t)7;
    uint32_t *sp  = (uint32_t *)top;

    /* Hardware-stacked exception frame (pushed by CPU on exception entry) */
    *(--sp) = 0x01000000u;        /* xPSR  — Thumb bit set                 */
    *(--sp) = (uint32_t)task_fn;  /* PC    — task entry point              */
    *(--sp) = 0xFFFFFFFDu;        /* LR    — EXC_RETURN: thread/PSP/no-FP */
    *(--sp) = 0;                  /* R12                                   */
    *(--sp) = 0;                  /* R3                                    */
    *(--sp) = 0;                  /* R2                                    */
    *(--sp) = 0;                  /* R1                                    */
    *(--sp) = (uint32_t)arg;      /* R0    — first function argument       */

    /* Software-stacked callee-saved regs (R4-R11), zeroed */
    sp -= 8;
    for (int i = 0; i < 8; ++i) sp[i] = 0;

    return sp; /* PendSV handler will LDMIA from here */
}

/* ------------------------------------------------------------------ */
/* Task lifecycle                                                        */
/* ------------------------------------------------------------------ */

void task_create(void (*task_function)(void *), const char *name,
                 uint32_t priority, uint32_t stack_size, void *arg)
{
    if (priority >= MAX_TASK_PRIORITIES) return;
    if (stack_size == 0 || stack_size > MAX_TASK_STACK_SIZE) return;

    struct TaskControlBlock *tcb = get_free_tcb();
    if (!tcb) return;

    size_t alloc_size = stack_size + PORT_STACK_GUARD_SIZE + (PORT_STACK_GUARD_SIZE - 1);
    void *alloc = malloc(alloc_size);
    if (!alloc) {
        free_tcb(tcb);
        return;
    }

    /* find a guard-aligned slot inside alloc */
    uintptr_t a = (uintptr_t)alloc;
    uintptr_t guard_base = (a + (PORT_STACK_GUARD_SIZE - 1)) & ~(uintptr_t)(PORT_STACK_GUARD_SIZE - 1);
    uint8_t *stack_base = (uint8_t *)(guard_base + PORT_STACK_GUARD_SIZE); /* usable stack start */

    /* Write canary at the bottom of the allocation (lowest address).
     * A stack overflow that grows downward will eventually clobber this word.
     * Checked on every context switch in port_switch_context(). */
    *((volatile uint32_t *)alloc) = 0xDEADBEEFU;

    memset(tcb, 0, sizeof(*tcb));
    tcb->stack_base    = stack_base;
    tcb->stack_alloc   = alloc;
    tcb->stack_size    = stack_size;
    tcb->stack_pointer = stack_init(stack_base, stack_size, task_function, arg);
    tcb->state         = TASK_READY;
    tcb->priority      = priority;
    tcb->task_function = task_function;
    tcb->arg           = arg;
    if (name) {
        strncpy(tcb->name, name, TASK_NAME_MAX_LEN - 1);
        tcb->name[TASK_NAME_MAX_LEN - 1] = '\0';
    }

    scheduler_add_task(tcb);
}

void task_delete(struct TaskControlBlock *tcb)
{
    if (!tcb) return;
    scheduler_remove_task(tcb);
    if (tcb->stack_alloc){
        free(tcb->stack_alloc);
        tcb->stack_alloc = NULL;
        tcb->stack_base = NULL;
    }
    free_tcb(tcb);
}

/* ------------------------------------------------------------------ */
/* Cooperative yield / timed delay                                      */
/* ------------------------------------------------------------------ */

void task_yield(void)
{
    port_trigger_pendsv();
}

void task_delay(uint32_t ticks)
{
    if (ticks == 0) return;
    struct TaskControlBlock *cur = scheduler_get_current();
    if (!cur) return;

    scheduler_sleep(cur, ticks);
    port_trigger_pendsv();       /* yield to next ready task */
}
