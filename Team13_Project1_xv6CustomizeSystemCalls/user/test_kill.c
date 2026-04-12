#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pid = fork();

  if(pid == 0){
    // child process
    while(1){
      printf("Child running...\n");
    }
  } else {
    // parent process
    for(int i = 0; i < 500000000; i++);
    // delay as sleep is not defined user
    printf("Parent sending kill signal\n");
    kill(pid);
    wait(0);
  }

  exit(0);
}
