//Jitendra-24JE0660
#include "kernel/types.h"
#include "user/user.h"

int main() {
  printf("Calling getprocinfo syscall...\n");
  getprocinfo();
  printf("Done!\n");
  exit(0);
}
