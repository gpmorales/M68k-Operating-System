/*	t3 -- Test Virtual P's and V's. t3 (consumer) and t4 (producer)
 *	exchange a message using the shared segment for synchronization
 *	and data transmission.
 */
#include	"h/tconst.h"
#include	"print.c"

int *hold = (int *)(SEG2);
int *empty = (int *)(SEG2 + 4);
int *full = (int *)(SEG2 + 8);
char *charbuff = (char *)(SEG2 + 12);

#define		MESSIZE	40

main()
{
	register int r2 asm("%d2");
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	char msg[MESSIZE], *p;

	print("t3 starts");

	/* initialize shared memory */
	*hold = 0;
	*full = 0;
	*empty = 1;

	/* block until t4 has started up */
	r4 = (int)hold;
	DO_VIRTP();
	print("t3 is free");

	/* receive characters from t4 */
	p = msg;
	do {
		r4 = (int)full;
		DO_VIRTP();
		*p = *charbuff;
		r4 = (int)empty;
		DO_VIRTV();
	} while (*p++ != '\0');

	/* print message received from t4 */
	print(msg);

	/* terminate normally */
	SYS17();
	print("t3 error: did not terminate");
	HALT();
}
