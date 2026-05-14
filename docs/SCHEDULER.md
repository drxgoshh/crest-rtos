# Scheduler

CREST uses a **priority-based preemptive scheduler with round-robin fairness** at each priority level.

---

## Priority

8 levels (0 = highest, 7 = lowest). The scheduler always runs the highest-priority ready task. A higher-priority task preempts immediately when it becomes ready.

The idle task sits at priority 7 and runs `wfi` in a loop — it only runs when nothing else is ready.

---

## Task states

```
READY ←──────────────────────┐
  │                          │
  └→ RUNNING → WAITING ──────┘
               (delay / sem / mutex expires)
```

- **READY** — runnable, waiting for the CPU
- **RUNNING** — currently on the CPU (exactly one task at a time)
- **WAITING** — blocked on a delay, semaphore, or mutex

---

## Round-robin

Tasks at the same priority share the CPU in turn. Each priority level has a pointer (`rr_current[p]`) tracking who ran last. On the next pick, the search starts from the task *after* that one, wrapping around. No task runs twice before others at the same level get a turn.

---

## When context switches happen

- **SysTick** fires every 1 ms → ticks down delays, then triggers PendSV
- **`task_delay()`** → marks task WAITING, triggers PendSV immediately
- **`task_yield()`** → stays READY, triggers PendSV to give others a turn
- **Mutex / semaphore** → block and trigger PendSV if the resource isn't available

---

## Context switch sequence (PendSV)

```
1. MRS R0, PSP                   — read current task's stack pointer
2. STMDB R0!, {R4-R11}           — push callee-saved regs onto task stack
3. BL port_switch_context         — C helper:
      save old SP → TCB
      mark old task READY (if it was RUNNING)
      pick next task (scheduler_get_next)
      configure MPU for new task
      restore CONTROL.nPRIV for new task
      return new SP
4. LDMIA R0!, {R4-R11}           — pop new task's callee-saved regs
5. MSR PSP, R0                   — set PSP to new task's stack
6. BX LR                         — exception return; hardware restores
                                    R0-R3, R12, LR, PC, xPSR from PSP
```

Hardware saves and restores R0–R3, R12, LR, PC, xPSR automatically. The software only handles R4–R11.

---

## Stack layout (per task)

Each task gets its own stack allocated from the kernel heap. The bottom 256 bytes are a **guard region** mapped as MPU NO_ACCESS — any overflow triggers a MemManage fault rather than silently corrupting memory.

```
high address  ┌─────────────────┐  ← initial SP
              │  xPSR / PC / LR │  (fake hardware frame, set by task_create)
              │  R0–R3, R12     │
              │  R4–R11         │  (fake software frame)
              │  (grows down)   │
              │                 │
low address   ├─────────────────┤  ← stack_base
              │  guard (256B)   │  NO_ACCESS
              └─────────────────┘
```
           - start = rr_current[0] = Task_C
           - t = Task_C->next = Task_A (wraps)
           - Task_A is READY → return Task_A
           - rr_current[0] = Task_A

T=4ms:   Pattern repeats: A → B → C → A → ...
```

All three tasks at priority 0 get equal CPU time: each runs every 3ms.

## Future Enhancements

- Add priority inheritance for mutexes
- Configurable timeslice per priority
- Per-task statistics (runtime, switches, blocked time)
- Deadlock detection for mutex chains
- Task suspension/resume support
