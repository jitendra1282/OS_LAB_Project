#ifndef PSTAT_H
#define PSTAT_H

#include "param.h"

struct pstat {
  int pid[NPROC];
  int state[NPROC];
  int priority[NPROC];
  int ticks[NPROC];
  int inuse[NPROC];
};

#endif
