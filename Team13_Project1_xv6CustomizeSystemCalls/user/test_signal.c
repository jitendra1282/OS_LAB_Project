#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int pid = fork();

  if(pid == 0){
    // Child process → runs continuously
    while(1){
      printf("Child running...\n");
     
      // small delay to avoid flooding output
      for(volatile int i = 0; i < 10000000; i++);
    }
  } else {
    // Parent process

    // Delay before sending first signal
    for(volatile int i = 0; i < 300000000; i++);

    printf("Parent: sending PAUSE signal\n");
    signal(pid, 10);   // pause

    // Delay
    for(volatile int i = 0; i < 300000000; i++);

    printf("Parent: sending RESUME signal\n");
    signal(pid, 12);   // resume

    // Delay
    for(volatile int i = 0; i < 300000000; i++);

    printf("Parent: sending KILL signal\n");
    signal(pid, 9);    // kill

    wait(0);
  }

  exit(0);
}
