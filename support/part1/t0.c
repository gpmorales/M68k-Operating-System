/* t0 -- Tests the memory management. */
#include	"h/tconst.h"
#include	"print.c"

main()
{
	register int r2 asm("%d2");
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	char i;
	int corrupt;

	print("t0 starts");

	/* write into the first word of pages 20-29 of seg2 */
	for (i = 20; i < 30; i++)
		*(int *)(SEG1 + i * PAGESIZE) = i;
	print("t0 ok: wrote to pages of seg 2");

	/* check if first word of pages still contain what we wrote */
	corrupt = FALSE;
	for (i = 20; i < 30; i++) {
		if (*(int *)(SEG1 + i * PAGESIZE) != i) {
			print("t0 error: swapper corrupted data");
			corrupt = TRUE;
			break;
		}
	}
	if (corrupt == FALSE)
		print("t0 ok: data survived swapper");

	/* try to access segment 0. Should cause termination */
	i = *(char *)2;
	print("t0 error: could access segment 0");
	HALT();
}
