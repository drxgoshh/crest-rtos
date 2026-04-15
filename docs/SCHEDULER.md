# CREST RTOS Scheduler Documentation

## Overview

The CREST scheduler is a **priority-based preemptive scheduler with round-robin (time-slice) fairness** at each priority level. Tasks are assigned a priority (0-7, where 0 is highest), and the scheduler always runs the highest-priority READY task. Among tasks at the same priority, scheduling is fair: each task gets a turn before any task runs twice.

## Key Concepts

### Priority Levels
- **8 priority levels** (0-7)
- Priority 0 = highest urgency
- Priority 7 = lowest urgency (typically idle task)
- **Preemptive**: A higher-priority task preempts lower-priority tasks immediately upon becoming READY
- **No aging**: Task priority is static; no dynamic priority boost

### Task States
```c
typedef enum {
    TASK_READY = 0,      /* runnable, waiting for CPU time */
    TASK_RUNNING,        /* currently executing on CPU */
    TASK_WAITING,        /* blocked: waiting for delay/semaphore/mutex */
    TASK_SUSPENDED       /* not in scheduler (reserved for future use) */
} task_state_t;
```

**State diagram:**
```
READY <---> RUNNING
  ^           |
  |           v
  +--- WAITING <--- (via task_delay/sem_wait/mutex_lock)
            |
            v
          READY (when delay expires or resource available)
```

### Round-Robin Scheduling

At each priority level, tasks are scheduled in **round-robin order**:

1. Tasks at the same priority form a circular linked list
2. The scheduler tracks the **last scheduled task** per priority (`rr_current[pr]`)
3. When `scheduler_get_next()` is called, it:
   - Starts searching from the task **after** the last scheduled one
   - Wraps around to the head when reaching the end of the list
   - Returns the first READY task it finds
   - Updates `rr_current[pr]` to this task

**Example:** Three tasks at priority 0: Task_A, Task_B, Task_C

```
Round 1: Run Task_A (rr_current[0] = Task_A)
         Next scan starts after Task_A → finds Task_B

Round 2: Run Task_B (rr_current[0] = Task_B)
         Next scan starts after Task_B → finds Task_C

Round 3: Run Task_C (rr_current[0] = Task_C)
         Next scan starts after Task_C → wraps to Task_A → finds Task_A

Round 4: Run Task_A (rr_current[0] = Task_A)
         ... cycle repeats
```

This ensures **no starvation** at the same priority: each task runs before any individual task runs twice.

## Scheduler Data Structures

### Task List Organization
```c
static struct TaskControlBlock *task_list[MAX_TASK_PRIORITIES];
```
- One singly-linked circular list per priority level
- All tasks at a priority are members of that list
- Tasks remain in their list their entire lifetime (not removed/re-inserted)
- State changes (`READY ↔ WAITING`) happen in-place

### Round-Robin Pointers
```c
static struct TaskControlBlock *rr_current[MAX_TASK_PRIORITIES];
```
- One pointer per priority level
- Points to the task that was last selected for CPU time
- Initialized to NULL; set to the list head on first selection
- Used by `scheduler_get_next()` to implement fairness

### Current Task Tracking
```c
static struct TaskControlBlock *current_task = NULL;
```
- Points to the task currently executing
- Updated by `scheduler_set_current()` in the context switch handler
- Used by `scheduler_get_current()` to identify the running task

## Public API

### Task Creation
```c
void task_create(void (*task_function)(void*), const char *name,
                 uint32_t priority, uint32_t stack_size, void *arg);
```
- Allocates a TCB from the pool
- Allocates stack memory from the heap
- Initializes the stack frame (simulating a suspended context)
- Adds task to `task_list[priority]` in TASK_READY state
- **Stack size** is checked: must be > 0 and ≤ `MAX_TASK_STACK_SIZE` (1024 bytes)

### Task Deletion
```c
void task_delete(struct TaskControlBlock *tcb);
```
- Removes task from `task_list[priority]`
- Deallocates stack memory via `free(tcb->stack_base)`
- Marks TCB slot as unused in the pool
- **Note:** Will be called during cleanup; not typically called at runtime by tasks

### Yielding Control
```c
void task_yield(void);
```
- Triggers a context switch via `trigger_pendsv()`
- Current task remains READY; scheduler will pick next task
- Used when a task wants to give other tasks a chance to run
- **Idle task** uses `wfi` (wait-for-interrupt) instead; doesn't need to yield

### Delay / Block
```c
void task_delay(uint32_t ticks);
```
- Block current task for `ticks` system ticks
- Marks current task as TASK_WAITING
- Sets `delay_ticks` counter
- Triggers context switch via `trigger_pendsv()`
- Task becomes READY again when `delay_ticks` reaches 0 (handled by `scheduler_tick()` in systick handler)

## Scheduler Core Functions

### `scheduler_get_next()` — Select the next task to run
```c
struct TaskControlBlock *scheduler_get_next(void);
```

**Algorithm:**
1. Lock interrupts via PRIMASK (prevent modification of task lists)
2. For each priority level (0 → MAX_TASK_PRIORITIES-1):
   - If the priority list is empty, skip to next priority
   - Determine starting point:
     - If `rr_current[pr]` is NULL (first time), start from list head
     - Otherwise, start from the task **after** `rr_current[pr]` (skip the last scheduled task)
   - Scan the circular list:
     - Find the first task with `state == TASK_READY`
     - Update `rr_current[pr]` to this task
     - Return it
   - If no READY task at this priority, continue to next priority
3. If no READY task found at any priority, return NULL (idle or error)
4. Unlock interrupts

**Key insight:** By starting the search after the last scheduled task, we ensure round-robin fairness without needing to move tasks in the list.

### `scheduler_tick()` — Update delay counters (called from SysTick handler)
```c
void scheduler_tick(void);
```

**Algorithm:**
1. For each priority level, for each task in that priority's list:
   - If task is `TASK_WAITING` and `delay_ticks > 0`, decrement it
   - When `delay_ticks` reaches 0, set state to `TASK_READY`
2. Does NOT trigger context switch (SysTick fires automatically every 1ms)

**Called from:** SysTick ISR (every `SysTick_interval` ms)
**Note:** Must use PRIMASK protection around state changes to avoid race conditions

### `scheduler_get_current()` and `scheduler_set_current()`
```c
struct TaskControlBlock *scheduler_get_current(void);
void scheduler_set_current(struct TaskControlBlock *tcb);
```
- Simple accessors for the current task pointer
- Used by context switch handler and synchronization primitives (mutex/semaphore)

## Context Switching Flow

### When Does a Context Switch Happen?

1. **Systick interrupt** fires every 1ms → calls `scheduler_tick()` and `trigger_pendsv()`
2. **Task calls `task_delay()`** → immediately triggers PendSV
3. **Task calls `task_yield()`** → immediately triggers PendSV
4. **Mutex/Semaphore operations** → block and schedule the scheduler

### The Context Switch Sequence (PendSV Handler)

Called when PendSV exception is pending. The naked assembly: handler saves R4-R11, calls the C helper, then restores R4-R11 and returns.

**Pseudocode:**
```
1. Save current task's callee-saved registers (R4-R11) to its stack
2. Call port_switch_context(old_sp):
   - Save old SP into current task's TCB
   - If current task was RUNNING, mark it TASK_READY
   - Call scheduler_get_next() to pick next task
   - Mark next task TASK_RUNNING
   - Update current_task pointer
   - Return new task's SP
3. Load new task's callee-saved registers (R4-R11) from its stack
4. Update PSP to new task's SP
5. Exception return: hardware restores R0-R3, R12, LR, PC, xPSR and resumes new task
```

## Priority Inversion & Synchronization

### Potential Issues

**Priority inversion** can occur when a high-priority task waits for a resource held by a low-priority task:

```
High-prio task T_H: blocks on mutex
Low-prio task T_L: holds mutex, but blocked by medium-prio task T_M
Result: T_H waits for T_L, but T_L can't run → T_H starves
```

### Mitigation Strategies (Not Currently Implemented)

- **Priority inheritance:** When T_L acquires the mutex and later gets blocked, boost T_L's priority to inherit T_H's
- **Priority ceiling:** Mutex has a ceiling priority; any task acquiring it temporarily runs at that ceiling
- **Separate ISR-safe APIs:** Semaphore `sem_post()` and mutex `mutex_unlock()` can be called from ISR without blocking; they only mark the task ready

## Example: Three Tasks at Same Priority

**Setup:**
```c
task_create(task_a, "A", 0, 512, NULL);
task_create(task_b, "B", 0, 512, NULL);
task_create(task_c, "C", 0, 512, NULL);
task_create(idle,   "Idle", 7, 256, NULL);
```

**Execution trace (assuming each task runs ~1ms before systick):**
```
T=0ms:   scheduler_get_next() → Task_A (first at priority 0)
         rr_current[0] = Task_A
         ...

T=1ms:   SysTick → scheduler_tick() [no delays expire] → trigger_pendsv()
         scheduler_get_next():
           - start = rr_current[0] = Task_A
           - t = Task_A->next = Task_B
           - Task_B is READY → return Task_B
           - rr_current[0] = Task_B
         Context switch to Task_B

T=2ms:   SysTick → trigger_pendsv()
         scheduler_get_next():
           - start = rr_current[0] = Task_B
           - t = Task_B->next = Task_C
           - Task_C is READY → return Task_C
           - rr_current[0] = Task_C

T=3ms:   SysTick → trigger_pendsv()
         scheduler_get_next():
           - start = rr_current[0] = Task_C
           - t = Task_C->next = Task_A (wraps)
           - Task_A is READY → return Task_A
           - rr_current[0] = Task_A

T=4ms:   Pattern repeats: A → B → C → A → ...
```

All three tasks at priority 0 get equal CPU time: each runs every 3ms.

## Testing & Debugging

### Verify Round-Robin Fairness
- Create 3+ tasks at same priority
- Have each print a message when they run
- Verify messages appear in round-robin order, not always the same task

### Check for Starvation
- Create a low-priority background task
- Create multiple higher-priority tasks
- Ensure lower-priority task eventually gets CPU time (or use `task_delay()` to verify state transitions)

### Debug Context Switches
- The PendSV handler calls `uart_write()` to log context switches
- Check output matches expected task order

### Priority Inversion Testing
- Create a low-priority task holding a mutex
- Create a high-priority task trying to acquire the same mutex
- Observe responsiveness (high-prio will be starved if low-prio is preempted by a medium-prio task)

## Limitations

1. **Static priority:** No dynamic priority boost or aging
2. **No priority inheritance:** Vulnerable to classic priority inversion
3. **Round-robin timeslice not configurable:** Fixed to ~1ms (SysTick interval)
4. **Small MAX_TASKS limit:** Only 8 tasks total (configurable but not dynamic)
5. **No task suspension:** TASK_SUSPENDED exists but is unused

## Future Enhancements

- Add priority inheritance for mutexes
- Configurable timeslice per priority
- Per-task statistics (runtime, switches, blocked time)
- Deadlock detection for mutex chains
- Task suspension/resume support
