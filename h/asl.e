#include "asl.h"

extern int insertBlocked(int *semAdd, proc_t *p);
extern proc_t *removeBlocked(int *semAdd);
extern proc_t *outBlocked(proc_t *p);
extern proc_t *headBlocked(int *semAddr);
extern void initSemd();
extern int headASL();