#include "types.h"

extern void createproc(state_t *state);
extern void killproc(state_t *state);
extern void semop(state_t *state);
extern void notused();
extern void trapstate(state_t *state);
extern void getcputime(state_t *state);