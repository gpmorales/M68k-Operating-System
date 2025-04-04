#define FALSE		0
#define TRUE		1
#define PAGESIZE	512
#define MICROSECONDS	150000

/* level 1 SYS calls */
#define	DO_READTERM	SYS9
#define	DO_WRITETERM	SYS10
#define	DO_VIRTV	SYS11
#define	DO_VIRTP	SYS12
#define	DO_DELAY	SYS13
#define	DO_DISKPUT	SYS14
#define	DO_DISKGET	SYS15
#define	DO_GETTOD	SYS16
#define	DO_TTERMINATE	SYS17

#define SEG0            0x000000
#define SEG1            0x080000
#define SEG2            0x100000
