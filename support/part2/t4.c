/*	t4 -- Test Virtual P's and V's. t3 (consumer) and t4 (producer)
 *	exchange a message using the shared segment for synchronization
 *	and data transmission.
 */
 register int r2 asm("%d2");
 register int r3 asm("%d3");
 register int r4 asm("%d4");

#include	"h/tconst.h"
#include	"print.c"

int *hold = (int *)(SEG2);
int *empty = (int *)(SEG2 + 4);
int *full = (int *)(SEG2 + 8);
char *charbuff = (char *)(SEG2 + 12);

main()
{
	int mysem;
	char *msg;

	print("t4 starts");

	/* give t3 a chance to start up and initialize shared memory */
	r4 = 1000000;
	DO_DELAY(); 		/* Delay for 1 second */

	r4 = (int)hold;
	DO_VIRTV();
	print("t4 is free");

	msg = "virtual synch OK";	/* message to be sent to t3 */

	/* send message to t3 */
	do {
		r4 = (int)empty;
		DO_VIRTP();
		*charbuff = *msg;
		r4 = (int)full;
		DO_VIRTV();
	} while (*msg++ != '\0');

	print("t4 finished sending");

	/* try to block on a private semaphore */
	mysem = 0;
	r4 = (int)&mysem;
	DO_VIRTP();			/* P(mysem) */
	/* should never reach here */
	print("t4 error: private sem block did not terminate");
	HALT();
}
