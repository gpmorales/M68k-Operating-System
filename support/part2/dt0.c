/*	t0 -- Test Disk Get and Disk Put, track 2 */
#include "h/tconst.h"
#include "print.c"

main()
{
	register int r2 asm("%d2");
        register int r3 asm("%d3");
        register int r4 asm("%d4");
	char buffer[PAGESIZE];
        char buferror[35];
	int word_no;
        int i;

	print("t0 starts");

	tcopy(buferror,"t0 error: disk i/o, sector:  ");
        for (i=0;i< 8; i+=2) {
		r4 = 2;
		* ((int *) buffer) = i;
		r2 = i;
		r3 = (int)buffer;
		DO_DISKPUT();
		if (r2 != PAGESIZE) {
			buferror[28]= (char) (48 + i); 
			print(buferror);
		}
        }

	print("t0: finished writing");

	tcopy(buferror,"t0 error: bad readback, sector:  ");

        for (i=0;i<8;i+=2) {
		r4 = 2;
		r2 = i;
		r3 = (int)buffer;
		DO_DISKGET();
		if (*(int *)buffer != i) {
			buferror[32]=(char) (48 + i);
			print(buferror);
		}
        }

        print("t0: finished reading");

	/* try to do a disk read into segment 0 */
	r3 = SEG0;
	DO_DISKGET();

	print("t0 error: just read into segment 0");
	HALT();
}

tcopy(a,b)
char *a, *b;
{

  while ((*a++ = *b++) != '\0');
}
