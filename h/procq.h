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
	int processor_time;			/* amount of processor time used by this process */

	state_t* prog_trap_old_state;  /* The area into which the processor state (the old state) is to be stored when a trap
								      occurs while running this process. The address of this area will be in D3 */
	state_t* prog_trap_new_state;  /* Holds the address for a full state_t structure 
									  containing the actual handler specifics, including the PC
									  for the handler routine captured from D4 in SYS5 */
	state_t* sys_trap_old_state; 
	state_t* sys_trap_new_state; 

	state_t* mm_trap_old_state; 
	state_t* mm_trap_new_state; 

	proc_t* parent_proc;
	proc_t* children_proc[MAXPROC];
	/*
		other entries defined by me
	*/
} proc_t;

#endif
