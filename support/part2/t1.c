/*	t1 -- Test of Delay and Get Time of Day */
#include	"h/tconst.h"
#include	"../../h/types.h"
#include	"print.c"

main()
{
	register int r2 asm("%d2");
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	int now1 ,now2;

	print("t1 starts");
	DO_GETTOD();
	now1 = r2;

	DO_GETTOD();
	now2 = r2;

	if (now2 < now1)
		print("t1 error: time decreasing");
	else
		print("t1 ok: time increasing");

	r4 = MICROSECONDS;	
	DO_DELAY();		
	DO_GETTOD();
	now1 = r2;

	if ((now1 - now2) < MICROSECONDS)
		print("t1 error: did not delay MICROSECONDS");
	else
		print("t1 ok: MICROSECONDS delay");

	SYS17();
	print("t1 error: sys17 did not terminate");
	HALT();
}
