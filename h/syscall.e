#include "types.h"

extern void createproc();
extern void killproc();
extern void semop();
extern void notused();
extern void trapstate();
extern void getcputime();
extern void trapsysdefault();

extern killprocrecurse(proc_t* process)