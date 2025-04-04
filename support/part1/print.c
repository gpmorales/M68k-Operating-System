
print(str)
char *str;
{
	register int r2 asm("%d2");
	register int r3 asm("%d3");
	register int r4 asm("%d4");
	char *s;

	r3 = (int)str;
	for (r4 = 0; str[r4] != '\0'; r4++);
	DO_WRITETERM();
	if (r2 < 0) {
		s = "Bad write term status";
		r3 = (int)s;
		r4 = 21;
		DO_WRITETERM();
		DO_TTERMINATE();
	}
}
