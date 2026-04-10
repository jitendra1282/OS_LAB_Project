// testshm.c — User-space demo for shared memory syscalls
// Member 1: Nandipati Jitendra Krishna Sri Sai
//
// Demonstrates shmget / shmattach / shmdetach:
//   1. Parent creates a shared segment (key=42).
//   2. Parent writes a message into the shared page.
//   3. Parent forks a child.
//   4. Child attaches the same segment (same key=42) and reads the message.
//   5. Child detaches and exits.
//   6. Parent detaches and exits.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SHM_KEY  42
#define SHM_SIZE 4096   // one page

int
main(void)
{
  printf("=== Shared Memory Demo (shmget/shmattach/shmdetach) ===\n");

  // ---- Parent: create shared segment ----
  int shmid = shmget(SHM_KEY, SHM_SIZE);
  if (shmid < 0) {
    printf("shmget failed\n");
    exit(1);
  }
  printf("[Parent] shmget(key=%d) -> shmid=%d\n", SHM_KEY, shmid);

  // ---- Parent: attach and write ----
  char *shared = (char *)shmattach(shmid);
  if ((uint64)shared == (uint64)-1) {
    printf("shmattach failed in parent\n");
    exit(1);
  }
  printf("[Parent] shmattach -> va=0x%p\n", shared);

  // Write a message into shared memory.
  char *msg = "Hello from Parent via Shared Memory!";
  int i;
  for (i = 0; msg[i] != '\0'; i++)
    shared[i] = msg[i];
  shared[i] = '\0';
  printf("[Parent] wrote: \"%s\"\n", shared);

  // ---- Fork ----
  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // ---- Child process ----
    int cshmid = shmget(SHM_KEY, SHM_SIZE);
    if (cshmid < 0) {
      printf("[Child] shmget failed\n");
      exit(1);
    }
    char *cshared = (char *)shmattach(cshmid);
    if ((uint64)cshared == (uint64)-1) {
      printf("[Child] shmattach failed\n");
      exit(1);
    }
    printf("[Child]  shmattach -> va=0x%p\n", cshared);
    printf("[Child]  read:  \"%s\"\n", cshared);

    // Child writes its own reply back.
    char *reply = "Hello back from Child!";
    int j;
    for (j = 0; reply[j] != '\0'; j++)
      cshared[j] = reply[j];
    cshared[j] = '\0';

    if (shmdetach(cshmid) < 0) {
      printf("[Child]  shmdetach failed\n");
      exit(1);
    }
    printf("[Child]  shmdetach OK — exiting\n");
    exit(0);
  }

  // ---- Parent waits for child ----
  int xstate;
  wait(&xstate);
  printf("[Parent] child exited with status %d\n", xstate);
  printf("[Parent] reads after child wrote: \"%s\"\n", shared);

  if (shmdetach(shmid) < 0) {
    printf("[Parent] shmdetach failed\n");
    exit(1);
  }
  printf("[Parent] shmdetach OK\n");
  printf("=== Shared Memory Demo Complete ===\n");
  exit(0);
}
