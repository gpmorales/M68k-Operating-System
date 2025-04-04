/*	t2 -- Test Disk Get and Disk Put */
#include "h/tconst.h"
#include "print.c"

#define MILLION	1000000

main()
{
	register int r2 asm("%d2");
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	char buffer[PAGESIZE];
	int word_no;

	print("t2 starts");
	buffer[0] = 'a';
	r4 = 3;
	r2 = 1;
	r3 = (int)buffer;
	DO_DISKPUT();
	if (r2 != PAGESIZE)
		print("t2 error: disk i/o result");
	else
		print("t2 ok: disk i/o result");

	buffer[0] = 'z';
	r4 = 1;
	r2 = 0;
	r3 = (int)buffer;
	DO_DISKPUT();

	r4 = 3;
	r2 = 1;
	r3 = (int)buffer;
	DO_DISKGET();
	if (buffer[0] != 'a')
		print("t2 error: bad first disk sector readback");
	else
		print("t2 ok: first disk sector readback");

	r4 = 1;
	r2 = 0;
	r3 = (int)buffer;
	DO_DISKGET();
	if (buffer[0] != 'z')
		print("t2 error: bad second disk sector readback");
	else
		print("t2 ok: second disk sector readback");

	/* should eventually exceed device capacity */
	r3 = (int)buffer;
	for (r4 = 2; r4<=MILLION; r4+=10) {
		r2 = 0;
		DO_DISKGET();
		if (r2 < 0)
			break;
	}
	if (r4 < MILLION)
		print("t2 ok: device capacity detection");
	else
		print("t2 error: device capacity undetected");

	/* try to do a disk read into segment 0 */
	r3 = SEG0;
	DO_DISKGET();

	print("t2 error: just read into segment 1");

	/* generate a variety of program traps */
	r4 = r4 / 0;
	print("t2 error: divide by 0 did not terminate");

	LDST(buffer);
	print("t2 error: priv. instruction did not terminate");

	SYS1();
	print("t2 error: sys1 did not terminate");
	HALT();
}
