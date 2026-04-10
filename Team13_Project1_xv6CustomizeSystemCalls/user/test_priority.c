#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pid = fork_with_priority(10);

  if(pid == 0){
    printf("Child process (priority = 10)\n");
  } else {
    printf("Parent process\n");
  }

  exit(0);
}
