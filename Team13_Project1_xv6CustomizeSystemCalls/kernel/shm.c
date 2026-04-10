// shm.c — Shared Memory IPC for xv6
// Member 1: Nandipati Jitendra Krishna Sri Sai
//
// Implements three system calls:
//   shmget(key, size)   -> shmid or -1
//   shmattach(shmid)    -> virtual address in user space or -1  
//   shmdetach(shmid)    -> 0 on success, -1 on error
//
// Design:
//   A fixed table of NSHMSEGS shared memory segments lives in the kernel.
//   Each segment has a key, a reference count, one physical page (4096 bytes),
//   and a spinlock for safe concurrent access.
//   shmget   – allocate or find a segment by key.
//   shmattach– map the segment's physical page into the calling process's
//              address space and return the virtual address.
//   shmdetach– unmap the segment from the process and decrement refcount;
//              free the physical page when refcount reaches zero.

#include "types.h"
#include "riscv.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define NSHMSEGS  16    // max number of shared memory segments
#define SHM_FREE   0
#define SHM_USED   1

struct shmseg {
  struct spinlock lock;
  int     state;        // SHM_FREE or SHM_USED
  int     key;          // caller-chosen identifier
  uint64  pa;           // physical page address (0 if not allocated)
  int     refcount;     // number of processes currently attached
};

static struct shmseg shm_table[NSHMSEGS];
static struct spinlock shm_table_lock;

// Called once at boot from main.c (via shminit() in defs.h).
void
shminit(void)
{
  initlock(&shm_table_lock, "shm_table");
  for (int i = 0; i < NSHMSEGS; i++) {
    initlock(&shm_table[i].lock, "shmseg");
    shm_table[i].state    = SHM_FREE;
    shm_table[i].key      = 0;
    shm_table[i].pa       = 0;
    shm_table[i].refcount = 0;
  }
}

// sys_shmget — find or create a shared memory segment.
// Argument 0: key  (int)
// Argument 1: size (int) — only used on creation; must be <= PGSIZE.
// Returns: shmid (0‥NSHMSEGS-1) on success, -1 on failure.
uint64
sys_shmget(void)
{
  int key, size;
  argint(0, &key);
  argint(1, &size);

  if (key < 0 || size <= 0 || size > PGSIZE)
    return -1;

  acquire(&shm_table_lock);

  // Search for an existing segment with this key.
  for (int i = 0; i < NSHMSEGS; i++) {
    if (shm_table[i].state == SHM_USED && shm_table[i].key == key) {
      release(&shm_table_lock);
      return i;   // return existing shmid
    }
  }

  // Allocate a new slot.
  for (int i = 0; i < NSHMSEGS; i++) {
    if (shm_table[i].state == SHM_FREE) {
      acquire(&shm_table[i].lock);
      shm_table[i].state    = SHM_USED;
      shm_table[i].key      = key;
      shm_table[i].refcount = 0;

      // Allocate one physical page for this segment.
      char *pa = kalloc();
      if (pa == 0) {
        shm_table[i].state = SHM_FREE;
        release(&shm_table[i].lock);
        release(&shm_table_lock);
        return -1;
      }
      memset(pa, 0, PGSIZE);
      shm_table[i].pa = (uint64)pa;

      release(&shm_table[i].lock);
      release(&shm_table_lock);
      return i;  // new shmid
    }
  }

  release(&shm_table_lock);
  return -1;  // table full
}

// sys_shmattach — map a shared segment into the current process.
// Argument 0: shmid (int)
// Returns: user virtual address of the mapped page, or -1 on error.
uint64
sys_shmattach(void)
{
  int shmid;
  argint(0, &shmid);

  if (shmid < 0 || shmid >= NSHMSEGS)
    return -1;

  struct shmseg *seg = &shm_table[shmid];
  acquire(&seg->lock);

  if (seg->state != SHM_USED || seg->pa == 0) {
    release(&seg->lock);
    return -1;
  }

  struct proc *p = myproc();

  // Place the shared page right after the current process heap.
  // Align to page boundary.
  uint64 va = PGROUNDUP(p->sz);

  // Map the physical page into the process page table (read+write, user).
  if (mappages(p->pagetable, va, PGSIZE, seg->pa, PTE_R | PTE_W | PTE_U) < 0) {
    release(&seg->lock);
    return -1;
  }

  // Extend process size to account for the newly mapped page.
  p->sz = va + PGSIZE;

  seg->refcount++;
  release(&seg->lock);

  return va;  // return virtual address to user
}

// sys_shmdetach — unmap a shared segment from the current process.
// Argument 0: shmid (int)
// The VA to unmap must match what shmattach returned; for simplicity
// we record no per-process VA — users are expected to pass the shmid
// and the runtime will unmap the last PGSIZE-aligned page below p->sz
// that maps to this segment's pa.
// Returns: 0 on success, -1 on error.
uint64
sys_shmdetach(void)
{
  int shmid;
  argint(0, &shmid);

  if (shmid < 0 || shmid >= NSHMSEGS)
    return -1;

  struct shmseg *seg = &shm_table[shmid];
  acquire(&seg->lock);

  if (seg->state != SHM_USED || seg->pa == 0) {
    release(&seg->lock);
    return -1;
  }

  struct proc *p = myproc();

  // Walk the process page table to find the VA that maps seg->pa.
  int found = 0;
  for (uint64 va = 0; va < p->sz; va += PGSIZE) {
    pte_t *pte = walk(p->pagetable, va, 0);
    if (pte == 0 || !(*pte & PTE_V))
      continue;
    uint64 phys = PTE2PA(*pte);
    if (phys == seg->pa) {
      // Unmap this page (do NOT free the physical memory — shared!).
      uvmunmap(p->pagetable, va, 1, 0);
      found = 1;
      break;
    }
  }

  if (!found) {
    release(&seg->lock);
    return -1;
  }

  seg->refcount--;

  // If no process is using this segment any more, free the physical page.
  if (seg->refcount <= 0) {
    kfree((void *)seg->pa);
    seg->pa       = 0;
    seg->state    = SHM_FREE;
    seg->key      = 0;
    seg->refcount = 0;
  }

  release(&seg->lock);
  return 0;
}
