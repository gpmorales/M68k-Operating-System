# Hoca OS

Hoca OS is a minimal operating system that implements kernel-level programming, process management, memory management, and I/O synchronization. 
It runs on a simulated MIPS-based architecture and includes essential kernel components written in C and assembly.

## Project Structure

### `h/`
This directory contains all header files for Hoca OS. These define essential constants, type definitions, structures, trap types, syscall codes, and hardware register definitions. All modules in the system rely on these shared definitions to maintain consistency and portability.

Key headers:
- `const.h`: Defines system constants (e.g., trap types, syscall numbers).
- `types.h`: Defines types like `state_t`, `proc_t`, and `sd_t` used throughout the system.
- `vpop.h`: Structures for batch semaphore operations.
- `page.h`, `support.h`: Definitions used for virtual memory and user-mode support code.

---

### `nucleus/`
The nucleus is the core kernel component responsible for:
- **Process scheduling**: Implements the round-robin scheduler and context switching.
- **Trap handling**: Handles exceptions and system calls using trap vectors.
- **Semaphore operations**: Provides basic P and V operations for synchronization.
- **Device management**: Interacts with terminal, printer, and disk devices via interrupt-driven handlers.

Files include:
- `main.c`: Entry point of the OS and kernel loop.
- `syscall.c`: Handles kernel-level system calls (`SYS1–SYS8`).
- `trap.c`: Contains trap handlers and dispatch logic.
- `int.c`: Device interrupt handlers and I/O completion routines.

---

### `support/`
The support module handles user-level system calls (`SYS9–SYS17`) on behalf of terminal processes. It runs in privileged mode but acts as a helper for user-mode T-processes.

Responsibilities include:
- Reading input from terminal (`readfromterminal`).
- Writing output to terminal (`writetoterminal`).
- Delaying execution (`delay`).
- Managing memory faults (`slmmhandler`).
- Terminating processes on request (`SYS17`).

Each T-process has its own support-level handler and stack defined in this module.

---

### `queues/`
Provides utility functions for managing process queues and blocking semaphores.

Features:
- Queue operations: insert, remove, head access.
- Semaphore block/unblock lists.
- Used by both the nucleus and support modules for process synchronization.

---

## Notes

- T-processes (user programs) run in virtual memory with support-level trap vectors for system interaction.
- Cron and daemon processes run in privileged mode and manage background tasks like waking delayed processes or swapping memory pages.
- The system ensures modularity between kernel (nucleus), user-level support, and hardware-level operations.

## Build & Run

Hoca OS is designed to run inside an emulator like `emacsim`.

---
