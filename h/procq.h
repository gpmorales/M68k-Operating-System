#ifndef PROCQ_H
#define PROCQ_H

/* link descriptor type */
typedef struct proc_link {
	int index;					/* index of the p_link/queue where the next proc_t is */
	struct proc_t* next;		/* next proc_t in particular queue */
} proc_link;

/* process table entry type */
typedef struct proc_t {
	proc_link p_link[SEMMAX];	/* pointers to next entries on queues */
	state_t p_s;				/* processor state of the process */
	int qcount;					/* number of queues containing this entry */
	int* semvec[SEMMAX];		/* vector of active semaphores for this entry */
	/*
		other entries defined by me
	*/
} proc_t;

#endif
