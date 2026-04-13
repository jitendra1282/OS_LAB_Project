#include "kernel/types.h"
#include "user/user.h"

int main() {
  printf("Testing thread_yield syscall...\n");
  printf("Before yield\n");
  thread_yield();
  printf("After yield\n");
  exit(0);
}