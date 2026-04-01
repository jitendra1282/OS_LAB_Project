# Team 13 — Project 1: xv6 Custom System Calls

**Course:** Operating Systems Lab (NSCS210/CSC211)  
**Department:** Computer Science and Engineering  
**Instructor:** Dr. Jaishree Mayank  

**Name:** Nandipati Jitendra Krishna Sri Sai  
**Admission No:** 24JE0660

---

## Group Information

| Member | Name |
|--------|------|
| 1  | Nandipati Jitendra Krishna Sri Sai |
| 2 | Member 2 Name |
| 3 | Member 3 Name |
| 4 | Member 4 Name |
| 5 | Member 5 Name |
| 6 | Member 6 Name |

---

## Project Overview

This project extends the **xv6 operating system** (RISC-V version) by adding custom system calls. xv6 is a simple Unix-like teaching OS developed by MIT. We modified its kernel to add new system calls and demonstrated their usage through user programs running inside xv6.

---

## Environment Setup

### Requirements
- macOS / Linux
- QEMU (RISC-V emulator)
- RISC-V GCC toolchain (`riscv64-elf-gcc`)
- Git

### Installation (macOS)
```bash
brew install qemu
brew install riscv-gnu-toolchain
```

### Clone and Run
```bash
git clone https://github.com/jitendra1282/OS_LAB_Project.git
cd OS_LAB_Project/Team13_Project1_xv6CustomizeSystemCalls
make TOOLPREFIX=riscv64-elf- qemu
```

To exit QEMU: press `Ctrl+A` then `X`

---

## System Call 1 — `getprocinfo` (Jitendra)

### What it does
`getprocinfo()` is a custom system call that fetches and displays information about the currently running process directly from the kernel. It prints the process name, PID (Process ID), and process state.

### Files Modified

| File | Change Made |
|------|-------------|
| `kernel/syscall.h` | Added `#define SYS_getprocinfo 22` |
| `kernel/syscall.c` | Registered `sys_getprocinfo` in the syscall table |
| `kernel/sysproc.c` | Wrote the actual syscall implementation |
| `user/user.h` | Declared `getprocinfo()` for user programs |
| `user/usys.pl` | Added entry point bridging user space to kernel |
| `user/testproc.c` | User test program that calls the syscall |
| `Makefile` | Added `_testproc` to UPROGS list |

### Implementation

**`kernel/sysproc.c`** — the core syscall logic:
```c
uint64 sys_getprocinfo(void) {
  struct proc *p = myproc();
  printf("Process name: %s\n", p->name);
  printf("Process PID: %d\n", p->pid);
  printf("Process state: %d\n", p->state);
  return 0;
}
```

**`user/testproc.c`** — user program to test the syscall:
```c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  printf("Calling getprocinfo syscall...\n");
  getprocinfo();
  printf("Done!\n");
  exit(0);
}
```

### How to Run
After booting xv6 with `make qemu`, type at the `$` prompt:
```
testproc
```

### Output
```
Calling getprocinfo syscall...
Process name: testproc
Process PID: 3
Process state: 4
Done!
```

### How it Works (Flow)
```
User program calls getprocinfo()
        ↓
usys.pl generates syscall trap instruction
        ↓
Kernel receives syscall number 22
        ↓
syscall.c dispatches to sys_getprocinfo()
        ↓
sysproc.c reads process struct via myproc()
        ↓
Prints name, PID, state back to user
```

---

## How to Add More System Calls (For Teammates)

Each teammate follows the same pattern. For your syscall number, use:
- Member 2: `#define SYS_yourname 23`
- Member 3: `#define SYS_yourname 24`
- Member 4: `#define SYS_yourname 25`
- Member 5: `#define SYS_yourname 26`
- Member 6: `#define SYS_yourname 27`

Edit the same 5 files: `syscall.h`, `syscall.c`, `sysproc.c`, `user.h`, `usys.pl`  
Create your own test file: `user/test_yourname.c`  
Add to Makefile: `$U/_test_yourname\`

---

## References
- [xv6 Book — MIT](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf)
- [xv6 Source — MIT GitHub](https://github.com/mit-pdos/xv6-riscv)
