#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
//  int i;
  int mask;

  if(argc < 3) {
    fprintf(2, "Usage: %s mask command [args]\n", argv[0]);
    fprintf(2, "Example: %s 32 grep hello README\n", argv[0]);
    exit(1);
  }

  mask = atoi(argv[1]);

  // Set the trace mask for this process and all children
  if(trace(mask) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }

  // Execute the command with its arguments
  // argv[0] is program name, argv[1] is mask, argv[2] is command, argv[3+] are command args
  exec(argv[2], &argv[2]);
  
  // If exec fails, print error
  fprintf(2, "%s: exec failed\n", argv[0]);
  exit(1);
}
