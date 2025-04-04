/*	t1 -- Test the recycling of  physical semaphores */

register int r2 asm("%d2");
register int r3 asm("%d3");
register int r4 asm("%d4");

#include	"h/tconst.h"
#include	"print.c"

int *hold = (int *)(SEG2);

#define		MESSIZE	40
#define		MAXPSEM	35

char buf[MESSIZE]="t1: iteration   ";

main()
{
	int i,j,k;
    int incr=0;

	print("t1 starts");

	/* initialize shared memory */
    /* (hold + 4*i) increments by 16, not 4 */
    
	for (i=1;i<2*MAXPSEM;i++)
	*(hold + 4*i) = 0;

	*(hold + 4*i) = 0;

	r4 = 5000000;
	DO_DELAY();

	/*  free t1 */
	r4 = (int)hold;
	DO_VIRTV();
	print("t1 is free");

	for (i=0;i<MAXPSEM; i++) {
		r4 = (int)hold + incr;
		DO_VIRTP();
        incr += 16;
		r4 = (int)hold + incr;
		DO_VIRTV();
        incr += 16;
		j=i;
		k=0;
		while (1) {
			j-=10;
			if (j<0) break;
			k++;
		}
		buf[15]= k + 48;
		buf[16]= j + 58;
		print(buf);
	} 

	print("t1 finishes normally");
	SYS17();
	print("t1 error: did not terminate");
	HALT();
}
