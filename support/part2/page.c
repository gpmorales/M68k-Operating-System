#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/vpop.h"

#define MAXFRAMES 20

register int r2 asm("%d2");
register int r3 asm("%d3");
register int r4 asm("%d4");

extern int end(), sem_mm, p_alive;

int sem_pf=1;
int pf_ctr, pf_start;
int Tsysframe[5];
int Tmmframe[5];
int Scronframe, Spagedframe, Sdiskframe;
int Tsysstack[5];
int Tmmstack[5];
int Scronstack, Spagedstack, Sdiskstack;

pageinit()
{
  int endframe;

  /* check if you have space for 35 page frames, the system
     has 128K */
  endframe=(int)end / PAGESIZE;
  if (endframe > 220 ) { /* 110 K */
    HALT();
  }

  Tsysframe[0] = endframe + 2;
  Tsysframe[1] = endframe + 3;
  Tsysframe[2] = endframe + 4;
  Tsysframe[3] = endframe + 5;
  Tsysframe[4] = endframe + 6;
  Tmmframe[0]  = endframe + 7;
  Tmmframe[1]  = endframe + 8;
  Tmmframe[2]  = endframe + 9;
  Tmmframe[3]  = endframe + 10;
  Tmmframe[4]  = endframe + 11;
  Scronframe  = endframe + 12;
  Spagedframe = endframe + 13;
  Sdiskframe  = endframe + 14;

  Tsysstack[0] = (endframe + 3)*512 - 2;
  Tsysstack[1] = (endframe + 4)*512 - 2;
  Tsysstack[2] = (endframe + 5)*512 - 2;
  Tsysstack[3] = (endframe + 6)*512 - 2;
  Tsysstack[4] = (endframe + 7)*512 - 2;
  Tmmstack[0]  = (endframe + 8)*512 - 2;
  Tmmstack[1]  = (endframe + 9)*512 - 2;
  Tmmstack[2]  = (endframe + 10)*512 - 2;
  Tmmstack[3]  = (endframe + 11)*512 - 2;
  Tmmstack[4]  = (endframe + 12)*512 - 2;
  Scronstack   = (endframe + 13)*512 - 2;
  Spagedstack  = (endframe + 14)*512 - 2;
  Sdiskstack   = (endframe + 15)*512 - 2;
  pf_start    = (endframe + 17);
 
/*
  pf_start = (int)end / PAGESIZE + 1; 
*/

  sem_pf = MAXFRAMES;
  pf_ctr = 0;
}

getfreeframe(term,page,seg)
int term, page, seg;
{
  int i;
  vpop vpopget[2];

  r4 = (int) &sem_pf;
  vpopget[0].op=LOCK;
  vpopget[0].sem=(int *)r4;
  r3=1;
  r4 = (int)vpopget;
  SYS3(); /* P(sem_pf) */

  r4 = (int) &sem_mm;
  vpopget[0].op=LOCK;
  vpopget[0].sem=(int *)r4;
  r3=1;
  r4 = (int)vpopget;
  SYS3();

  i = pf_ctr++ + pf_start;

  r4 = (int) &sem_mm;
  vpopget[0].op=UNLOCK;
  vpopget[0].sem=(int *)r4;
  r3=1;
  r4 = (int)vpopget;
  SYS3();
 
  return (i);
} 

pagein(term,page,seg,pf)
int term, page, seg, pf;
{
  HALT();
}

putframe(term)
int term;
{
}
