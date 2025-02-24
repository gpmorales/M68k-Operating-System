#include "../interrupts/int.c"
#include "../../h/types.h"				
#include "../../h/procq.e"				
#include "../../h/main.e"				

/*
	Definition of System call routines SYS1, SYS2, SYS3, SYS4, SYS5 and SYS6.
	These are executable only by processes running in supervisor mode. 
	If invoked in usermode, a privileged instruction trap should be generated.
*/


/*
	When executed, this instruction causes a new process, said to be a progeny of the first, to be created.

	D4 will contain the address of a processor state area at the time this instruction is executed.
	This processor state should be used as the initial state for the newly created process.
	
	The process executing the SYS1 instruction continues to exist and to execute.
	If the new process cannot be created due to lack of resources (for example no more entries in the process table), an error code of −1 is returned in D2.
	Otherwise, D2 contains zero upon return.
*/
void createproc()
{
	// The invoking process is to be the father of this process. Get the invoking process state via SYS_OLD_STATE_AREA
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Create a child process
	proc_t* child_proc = allocProc();

	// Cannot create new process due to lack of resources
	if (child_proc == (proc_t*)ENULL) {
		SYS_TRAP_OLD_STATE->s_r[2] = -1;
	}
	else {
		// Child process can be created
		SYS_TRAP_OLD_STATE->s_r[2] = 0;

		// Set the child's processor state
		state_t* child_proc_state = (state_t*)SYS_TRAP_OLD_STATE->s_r[4];

		// Hardware will have loaded the appropiate info in SYS_OLD_STATE_AREA
		int trap_type = SYS_TRAP_OLD_STATE->s_r[2];
		child_proc->p_s = *child_proc_state;

		// Grab the parent process
		proc_t* parent_proc = headQueue(ready_queue);
		child_proc->parent_proc = parent_proc;

		// Insert child into parent children list
		int i;
		for (i = 0; i < MAXPROC; i++) {
			if (parent_proc->children_proc[i] == (proc_t*)ENULL) {
				parent_proc->children_proc[i] = child_proc;
				break;
			}
		}

		// Add child process to the tail of the ready queue
		insertProc(&ready_queue, child_proc);
	}
}


/*
	Apply this to the calling process and all its descendants.
	Remove it from all semaphore queues (OutBlocked) and the RQ.
	You will need a recursive function to descend the process tree, deleting each descendant and updating the tree.
*/
void killproc() 
{
	// Get the invoking process from the RQ
	proc_t* proc = headQueue(ready_queue);

	// Remove the current invoking process from the RQ and all ASL queues
	removeProc(&ready_queue);
	outBlocked(proc);

	// Recurse through all children processes 
	killchildrenproc(proc);

	// Remove all progeny and parent process links
	resetProcess(proc);
}


// Recursive function to terminate process and its children
void killchildrenproc(proc_t* proc)
{
	int i;
	for (i = 0; i < MAXPROC; i++) {
		proc_t* child_proc = proc->children_proc[i];
		// Has active child process
		if (child_proc != (proc_t*)ENULL) {
			// Remove child from all queues in the ASL and from RQ if possible
			outBlocked(child_proc); 
			outProc(&ready_queue, child_proc);

			// Recurse through all children processes 
			killchildrenproc(child_proc);

			// Remove all progeny and parent process links
			resetProcess(child_proc);
		}
		else break;
	}
}


void semop()
{

}


/*
	If this instruction is executed, it should should result in an error condition. Your
	nucleus should halt if this instruction is executed.
*/
void notused() {
	//HALT();
}


// Called internally to handle traps!???????????????????
/*
	When this instruction is executed, it supplies three pieces of information to the nucleus:
	  • The type of trap for which a trap state vector is being established. This information will be placed in D2 at the time of the call, using the following encoding:
		0-program trap
		1-memory management trap
		2-SYS trap

	  • The area into which the processor state (the old state) is to be stored when a trap
	    occurs while running this process. THE ADDRESS OF THIS AREA WILL BE IN D3.

	  • The processor state area that is to be taken as the new processor state if a trap
	    occurs while running this process. THE ADDRESS OF THIS AREA WILL BE IN D4.

	The nucleus, on execution of this instruction, should save the contents of D3 and D4
	(in the process table entry) to pass up the appropriate trap when (and if) it occurs while running this process.
*/
void trapstate()
{
	// This function is invoked as a SYS call trap. Hence the interrupted process's state is saved in SYS_TRAP_OLD_STATE
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process so we populate the corresponding old processor state area with whats in 0x930
	proc_t* process = headQueue(ready_queue);

	// Hardware will have loaded the appropiate info in SYS_OLD_STATE_AREA
	int trap_type = SYS_TRAP_OLD_STATE->s_r[2];
	state_t* old_state_area = (state_t*)SYS_TRAP_OLD_STATE->s_r[3];	  // address to the old state area we will populate later with the old processor state
	state_t* new_state_area = (state_t*)SYS_TRAP_OLD_STATE->s_r[4];   // address to the specific handler processor state for this trap, already populated!

	// Make sure in proct, ensure these pointer havent been popoulated (SYS5 wasnt populated)
	// check type of trap, then i set
	// ensure the proc_t xx_old_state_area and xx_new_state_are

	if (trap_type == PROGTRAP) {
		// We can only specify the trap state vector once per trap type
		if (process->prog_trap_old_state != (state_t*)ENULL && process->prog_trap_new_state != (state_t*)ENULL) {
			process->prog_trap_old_state = old_state_area;
			process->prog_trap_new_state = new_state_area;
		}
		else {
			killproc();
		}
	}
	else if (trap_type == MMTRAP) {
		// We can only specify the trap state vector once per trap type
		if (process->mm_trap_old_state != (state_t*)ENULL && process->mm_trap_new_state != (state_t*)ENULL) {
			process->mm_trap_old_state = old_state_area;
			process->mm_trap_new_state = new_state_area;
		} 
		else {
			killproc();
		}
	}
	else if (trap_type == SYSTRAP) {
		// We can only specify the trap state vector once per trap type
		if (process->sys_trap_old_state != (state_t*)ENULL && process->sys_trap_new_state != (state_t*)ENULL) {
			process->sys_trap_old_state = old_state_area;
			process->sys_trap_new_state = new_state_area;
		} 
		else {
			killproc();
		}
	}

	// Update the processor state by setting the proc_t state ps to the OLD SYS PROCESS STATE area
	process->p_s = *SYS_TRAP_OLD_STATE;

	// Call schedule to exit the kernel routine and continue running the interrupted process or next process
	schedule();
}


/*
	When this instruction is executed, it causes the CPU time (in microseconds) used by
	the process executing the instruction to be placed in D2. This means that the
	nucleus must record (in the process table entry) the amount of processor time used
	by each process.
*/
void getcputime()
{
	// This function is invoked as a SYS call trap. Hence the interrupted process's state is saved in SYS_TRAP_OLD_STATE
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Hardware will have loaded the appropiate info in SYS_OLD_STATE_AREA
	int processor_time = SYS_TRAP_OLD_STATE->s_r[2];

	// Grab the interrupted process
	proc_t* process = headQueue(ready_queue);

	// Update the process's time the CPU
	process->processor_time = processor_time;
}
// Each proct_ needs a new field to keep track of how much cpu time has been used.This call just returns the value stored.?????????????
