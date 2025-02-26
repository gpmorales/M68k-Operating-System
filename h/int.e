#include "types.h"

extern void intinit();
extern void waitforpclock(state_t *old);
extern void waitforio(state_t *old);
extern void intdeadlock();
extern void intschedule();
