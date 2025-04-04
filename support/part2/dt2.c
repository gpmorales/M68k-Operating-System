/*	t2 -- Test Disk Get and Disk Put, track 4 */
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

	print("t2 starts");

	tcopy(buferror,"t2 error: disk i/o, sector:  ");
        for (i=0;i< 8; i+=2) {
		r4 = 4;
		* ((int *) buffer) = i+2;
		r2 = i;
		r3 = (int)buffer;
		DO_DISKPUT();
		if (r2 != PAGESIZE) {
			buferror[28]= (char) (48 + i); 
			print(buferror);
		}
        }

	print("t2: finished writing");

	tcopy(buferror,"t2 error: bad readback, sector:  ");

        for (i=0;i<8;i+=2) {
		r4 = 4;
		r2 = i;
		r3 = (int)buffer;
		DO_DISKGET();
		if (*(int *)buffer != i+2) {
			buferror[32]=(char) (48 + i);
			print(buferror);
		}
        }

	print("t2: finished reading");

	/* try to do a disk read into segment 1 */
	r3 = SEG0;
	DO_DISKGET();

	print("t2 error: just read into segment 1");
	HALT();
}

tcopy(a,b)
char *a, *b;
{

  while ((*a++ = *b++) != '\0');
}
