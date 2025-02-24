#include "procq.h"

extern void insertProc(proc_link* tp, proc_t* p);
extern proc_t* removeProc(proc_link* tp);
extern proc_t* outProc(proc_link* tp, proc_t* p);
extern proc_t* allocProc();
extern void freeProc(proc_t* p);
extern void initProc();
extern proc_t* headQueue(proc_link tp);
