/*	t1 -- Test Disk Get and Disk Put, track 98 */
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

	print("t1 starts");

	tcopy(buferror,"t1 error: disk i/o, sector:  ");
        for (i=0;i< 8; i+=2) {
		r4 = 98;
		* ((int *) buffer) = i+1;
		r2 = i;
		r3 = (int)buffer;
		DO_DISKPUT();
		if (r2 != PAGESIZE) {
			buferror[28]= (char) (48 + i); 
			print(buferror);
		}
        }

	print("t1: finished writing");

	tcopy(buferror,"t1 error: bad readback, sector:  ");

        for (i=0;i<8;i+=2) {
		r4 = 98;
		r2 = i;
		r3 = (int)buffer;
		DO_DISKGET();
		if (*(int *)buffer != i+1) {
			buferror[32]=(char) (48 + i);
			print(buferror);
		}
        }

	print("t1: finished reading");

	/* try to do a disk read into segment 1 */
	r3 = SEG0;
	DO_DISKGET();

	print("t1 error: just read into segment 1");
	HALT();
}

tcopy(a,b)
char *a, *b;
{

  while ((*a++ = *b++) != '\0');
}
