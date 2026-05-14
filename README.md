<p align="center">
  <img src="crest.png" alt="CREST Logo" width="320"/>
</p>

<h1 align="center">CREST</h1>
<p align="center"><em>Compact Real-time Embedded SysTem</em></p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C-blue?logo=c"/>
  <img src="https://img.shields.io/badge/Type-RTOS-orange"/>
  <img src="https://img.shields.io/badge/arch-ARM%20Cortex--M4-blueviolet"/>
  <img src="https://img.shields.io/badge/license-MIT-green"/>
</p>

---

## What is this?

Hey, I'm Dragos — this is my attempt to truly understand how operating systems work by building one myself from scratch for a real microcontroller.

No HAL. No newlib. No abstractions hiding the interesting parts. Just bare metal C, a linker script, and ARM reference manuals. Every component — the scheduler, allocator, mutex, context switch, MPU isolation — I either wrote myself or worked through until I understood it.

---

## What's implemented

| Component | Description |
|---|---|
| **Preemptive scheduler** | Priority-based (0–7) with round-robin fairness at each level |
| **Context switch** | Naked PendSV handler; saves/restores R4–R11 on the process stack |
| **Heap allocator** | First-fit free list with split, coalesce, and critical section protection |
| **Task management** | `task_create` / `task_delete` / `task_delay` / `task_yield` |
| **Mutex** | Binary mutex with owner tracking and wait list |
| **Semaphore** | Counting semaphore; `sem_give` is ISR-safe |
| **Queue** | Fixed-size item FIFO with blocking push/pop |
| **Userspace isolation** | MPU-enforced memory protection + SVC syscall gate |
| **Kernel objects** | Generation-tagged 32-bit handles (queue, sem, mutex) |
| **UART logger** | Polling UART on USART2 (PA2/PA3), 115200 8N1 |
| **Custom libc** | `memset`, `memcpy`, `strncpy`, `snprintf` — no newlib |

---

## Project structure

```
crest-rtos/
├── arch/arm/cortex-m4/     # PendSV, SVC handler, MPU, port_start_first_task
├── boards/stm32f446/       # Startup, linker scripts, UART driver
├── cmake/
│   ├── toolchains/         # arm-none-eabi GCC toolchain
│   └── boards/             # Per-board CPU flags and sources
├── kernel/
│   ├── include/            # Public headers
│   ├── task.c / sched.c    # Task lifecycle and scheduler
│   ├── alloc.c             # Kernel heap allocator
│   ├── user_alloc.c        # User heap allocator (16KB)
│   ├── kobject.c           # Kernel object handle table
│   ├── mutex.c / semaphore.c / queue.c
│   └── syscall_stubs.c     # Privilege-checking API wrappers
└── docs/
    ├── SCHEDULER.md
    └── USERSPACE.md
```

---

## Building

Requires `cmake >= 3.16` and `arm-none-eabi-gcc`.

```bash
./build.sh          # build
./build.sh -f       # build + flash via ST-Link
./build.sh -B       # clean rebuild
./build.sh -h       # all options
```

---

## Adding a new board

1. Create `cmake/boards/<name>.cmake` — set `CREST_ARCH`, `CREST_CPU_FLAGS`, `CREST_LINKER_SCRIPT`, `CREST_BOARD_SOURCES`, `CREST_BOARD_INCLUDES`
2. Create `boards/<name>/` with at minimum `startup.c`, `link.ld`, and a UART driver
3. Pass `-b <name>` to `build.sh`

---

## Contributing

Issues, discussions, and PRs are welcome. This is a learning project — no contribution is too small.
