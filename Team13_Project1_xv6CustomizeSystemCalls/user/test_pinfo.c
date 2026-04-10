#include "kernel/types.h"
#include "user/user.h"
#include "kernel/pstat.h"

int main() {
  struct pstat ps;

  getpinfo(&ps);

  for(int i = 0; i < NPROC; i++){
    if(ps.inuse[i]){
      printf("PID: %d | State: %d | Priority: %d | Ticks: %d\n",
        ps.pid[i],
        ps.state[i],
        ps.priority[i],
        ps.ticks[i]);
    }
  }

  exit(0);
}
