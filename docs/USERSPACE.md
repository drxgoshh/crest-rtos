# Userspace Isolation

CREST supports running tasks in unprivileged mode with MPU-enforced memory protection. Kernel objects are accessed via handles through a syscall gate, so user tasks can never touch kernel memory directly.

---

## Memory layout

The MPU is programmed with five regions on every context switch:

```
0x08000000  ┌──────────────────────────┐
            │  Flash (512KB)           │  r0: user = read + execute
            │  kernel + user code      │
0x08080000  └──────────────────────────┘

0x20000000  ┌──────────────────────────┐
            │  Kernel heap / globals   │  r1: user = NO ACCESS
            │                          │
            │  ┌────────────────────┐  │
            │  │  User heap (16KB)  │  │  r3: user = RW (overrides r1)
            │  └────────────────────┘  │
            │  ┌────────────────────┐  │
            │  │  Task stack        │  │  r2: user = RW (overrides r1)
            │  │  [256B guard]      │  │  r7: NO ACCESS (overrides r2)
            │  └────────────────────┘  │
0x20020000  └──────────────────────────┘

0x40000000      Peripherals             (not in any user region → fault)
```

A user task can read/execute flash, read/write its own stack and the user heap, and nothing else. Any other access — kernel globals, another task's stack, UART registers — triggers a MemManage fault.

---

## Privilege levels

The CPU has two thread-mode privilege levels controlled by `CONTROL.nPRIV`:

| `nPRIV` | Mode | Can access |
|---|---|---|
| 0 | Privileged | Everything |
| 1 | Unprivileged | Only MPU-allowed regions |

Handler mode (SVC, PendSV, SysTick) is always privileged regardless of `nPRIV`.

Each TCB stores `TASK_FLAG_USER` in its `flags` field. On every context switch, PendSV writes `CONTROL.nPRIV` from the incoming task's flags — so privilege is per-task, not global.

A task drops to userspace by calling `port_set_unprivileged()`, which sets the flag and clears `nPRIV`. It cannot return to privileged mode.

---

## Syscall gate

User tasks can't call kernel functions directly (the kernel's `.data`/`.bss` is in the NO ACCESS region). Instead they go through SVC:

```
user task (unprivileged, PSP)
│
│  queue_push(handle, data, timeout)
│    └─ port_is_privileged() == false
│         └─ port_syscall_invoke(SYSCALL_QUEUE_SEND, handle, data, timeout, 0)
│              mov r12, r0   ← id into R12
│              mov r0,  r1   ← arg0
│              mov r1,  r2   ← arg1
│              mov r2,  r3   ← arg2
│              SVC #0
│                │
│                │  CPU atomically:
│                │    stacks R0–R3, R12, LR, PC, xPSR → PSP
│                │    switches to MSP, enters handler (privileged)
│                ▼
│         SVC_Handler
│           read PSP → frame pointer
│           id     = frame[4]  ← stacked R12
│           arg0   = frame[0]  ← stacked R0 (handle)
│           arg1   = frame[1]  ← stacked R1 (data ptr)
│           arg2   = frame[2]  ← stacked R2 (timeout)
│           → validate handle → call z_impl_queue_push
│           frame[0] = return value
│                │
│                │  CPU restores frame from PSP, returns to user task
│                ▼
└─ result in R0
```

The syscall ID goes in R12 because the Cortex-M hardware stacks R12 at exception entry — it's safe to read from `frame[4]` without any register-file tricks.

---

## Kernel object handles

User tasks hold **32-bit opaque handles** instead of pointers. A handle encodes:

```
bits [31:16]  generation counter
bits [15:0]   table index
```

The kernel maintains a table of 32 slots. On every syscall, `k_obj_get(handle, type)` validates the index, generation, and type before returning the real pointer. A stale or forged handle always fails safely.

```c
uint32_t qh = queue_create(32, 4);   // privileged — returns handle
// ...
port_set_unprivileged();

queue_push(qh, msg, 100);            // unprivileged → SVC → kernel validates qh
```

---

## API summary

```c
// Drop to unprivileged mode (one-way)
void port_set_unprivileged(void);

// Privilege-checking wrappers (work from both modes)
int      queue_push(uint32_t handle, const void *data, uint32_t timeout_ms);
int      queue_pop(uint32_t handle, void *data, uint32_t timeout_ms);
void     sem_give(uint32_t handle);
void     sem_take(uint32_t handle);
void     mutex_lock(uint32_t handle);
void     mutex_unlock(uint32_t handle);
void     task_delay(uint32_t ticks);
void     k_uart_write(const char *s);   // safe UART from userspace
```

When called privileged → direct kernel call. When called unprivileged → SVC.
