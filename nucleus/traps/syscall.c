#include "../../h/types.h"
#include "../../h/const.h"
#include "../../h/procq.e"
#include "../../h/asl.e"
#include "../../h/vpop.h"

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

extern proc_link readyQueue;
extern void schedule();
void killprocrecurse(proc_t* p);


void createproc()
{
	// Grab the interrupted process from the RQ, serving as the parent process
	proc_t* parentProcess = headQueue(readyQueue);

	// Get its processor state via SYS_OLD_STATE_AREA
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Create a child process
	proc_t* childProcess = allocProc();

	// Cannot create new process due to lack of resources or another reason
	if (childProcess == (proc_t*)ENULL) {
		// Return -1 in s_r D2
		SYS_TRAP_OLD_STATE->s_r[2] = -1;
	}
	else {
		// Child process can be created
		SYS_TRAP_OLD_STATE->s_r[2] = 0;

		// Set the child's processor state
		state_t* childProcState = (state_t*)SYS_TRAP_OLD_STATE->s_r[4];
		childProcess->p_s = *childProcState;

		// Update the parent process
		childProcess->parent_proc = parentProcess;

		// Insert child into parent children list
		if (parentProcess->children_proc == (proc_t*)ENULL) {
			parentProcess->children_proc = childProcess;
		}
		else {
			// Iterate through the parent's children to add the new Process as a sibling
			proc_t* siblingProces = parentProcess->children_proc;
			while (siblingProces->sibling_proc != (proc_t*)ENULL) {
				siblingProces = siblingProces->sibling_proc;
			}
			siblingProces->sibling_proc = childProcess;
		}

		// Add child process to the tail of RQ
		insertProc(&readyQueue, childProcess);
	}

	// Update the parent process's processor state by setting the ps field to the OLD SYS PROCESS STATE area
	parentProcess->p_s = *SYS_TRAP_OLD_STATE;

	// Call schedule to exit this kernel routine and switch the execution flow back to the interrupted process OR the next proc on the RQ
	schedule();
}


// Recursive function to terminate process and its children
void killprocrecurse(proc_t* process)
{
	if (process == (proc_t*)ENULL) {
		return;
	}

	// DFS into the last child, then go for siblings of those children
	proc_t* childProcess = process->children_proc;
	while (childProcess != (proc_t*)ENULL) {
		proc_t* nextSibling = childProcess->sibling_proc;
		killprocrecurse(childProcess);
		childProcess = nextSibling;
	}

	// Backtrack and remove all of the process's progeny links and from ASL queues
	if (outBlocked(process) == (proc_t*)ENULL) {
		outProc(&readyQueue, process);
	}
	else {
		int i;
		for (i = 0; i < SEMMAX; i++) {
			if (process->semvec[i] != (int*)ENULL) {
				*(process->semvec[i]) = *(process->semvec[i]) + 1;
			}
		}
	}
	freeProc(process);
}


/*
	Apply this to the calling process and all its descendants.
	Remove it from all semaphore queues (OutBlocked) and the RQ.
	You will need a recursive function to descend the process tree, deleting each descendant and updating the tree.
*/
void killproc() 
{
	// Get the about-to-be terminated process from the RQ
	proc_t* killProcess = headQueue(readyQueue);

	// Set the parent child pointer to the killed process's immeadiate sibling
	proc_t* parentProcess = killProcess->parent_proc;
	if (parentProcess != (proc_t*)ENULL) {
		parentProcess->children_proc = killProcess->sibling_proc;
	}

	// Kill family tree
	killprocrecurse(killProcess);

	// Call schedule to exit this kernel routine and switch the execution flow back to the interrupted process OR the next proc on the RQ
	schedule();
}


/*
	When this instruction is executed, it is interpreted by the nucleus as a set of V and P operations atomically applied to a group of semaphores.
	Each semaphore and corresponding operation is described in a vpop structure. The vpop structure is defined in "vpop.h".
	D4 contains the address of the vpop vector, and D3 contains the number of vpops in the vector

	- The P’s may or may not get the calling process stuck on a semaphore Q, if so it comes off the RQ
	– The V’s on active semaphores will remove the process at the head of that Sem PTE Q, but only puts it back on the
	  RQ if its not on additional semaphore Q’
	– Returns to the process now at the head of the RQ.
*/
void semop()
{
	// The interrupted process's state is saved in SYS_TRAP_OLD_STATE, which has the vpop vector address in D3
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Get the interrupted process's processor state
	proc_t* callingProcess = headQueue(readyQueue);

	// Get Vector of operatons to perform on a semaphores
	vpop* semOperations = (vpop*)SYS_TRAP_OLD_STATE->s_r[4];
	int len = SYS_TRAP_OLD_STATE->s_r[3];

	int callingProcessBlocked = FALSE;

	// Iterate thorugh each entry and peform action on semaphore with given address based on the operation type
	int i;
	for (i = 0; i < len; i++) {
		int op = semOperations[i].op;			// Get operation to be performed on semaphore
		int* semAddr = semOperations[i].sem;	// Get the semaphore address
		int prevSemVal = *semAddr;				// Get the semaphore proper
		*semAddr = prevSemVal + op;				// Update the semaphore

		// V (+1) on an active semaphore (-value) means a resource has been freed, allowing the next blocked process on that Semaphore to maybe be put back on RQ
		if (op == UNLOCK) {
			if (prevSemVal < 0) {
				// Remove the process at the head of the corresponding Semaphore Queue and update Semvec
				proc_t* process = removeBlocked(semAddr);

				// If the process is no longer blocked on any Semaphores, then add it back to the RQ
				if (process != (proc_t*)ENULL && process->qcount == 0) {
					insertProc(&readyQueue, process);
				}
			}
			else {
				// Do nothing if the semaphore was not active, as resources are already free
			}
		}
		// P (-1) will decrement the semaphore, if the sem value is negative afterwards, the interrupted process should be blocked
		else if (op == LOCK) {
			if (prevSemVal <= 0) {
				// Semaphore has become negative, meaning its blocking at least the process and is now active
				// The running process at the head of the Queue can be blocked by a P operation 
				// we do not want this to prevent other processes from being unblocked, so use a flag
				insertBlocked(semAddr, callingProcess);
				callingProcessBlocked = TRUE;
			}
			else {
				// Do nothing if the semaphore still has resources
			}
		}
	}

	if (callingProcessBlocked) {
		// Update the processor state by setting the proc_t state ps to the OLD SYS PROCESS STATE area
		callingProcess->p_s = *SYS_TRAP_OLD_STATE;
		removeProc(&readyQueue);

		// Call schedule to exit this kernel routine and switch the execution flow back the interrupted process
		schedule();
	}
}


/*
	If this instruction is executed, it should should result in an error condition. Your
	nucleus should halt if this instruction is executed.
*/
void notused() {
	HALT();
}


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
	// The interrupted process's state is saved in SYS_TRAP_OLD_STATE
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process so we populate the corresponding old processor state area with whats in 0x930
	proc_t* process = headQueue(readyQueue);

	// Hardware will have loaded the appropiate info in SYS_OLD_STATE_AREA
	int trapType = SYS_TRAP_OLD_STATE->s_r[2];
	state_t* old_state_area = (state_t*)SYS_TRAP_OLD_STATE->s_r[3];	  // address to the old state area we will populate later with the old processor state
	state_t* new_state_area = (state_t*)SYS_TRAP_OLD_STATE->s_r[4];   // address to the specific handler processor state for this trap, already populated!

	// Make sure the corresponding pointers in proc_t have NOT been popoulated (SYS5 wasnt invoked)

	if (trapType == PROGTRAP) {
		// We can only specify the trap state vector once per trap type
		if (process->prog_trap_old_state == (state_t*)ENULL && process->prog_trap_new_state == (state_t*)ENULL) {
			process->prog_trap_old_state = old_state_area;
			process->prog_trap_new_state = new_state_area;
		}
		else {
			killproc();
		}
	}
	else if (trapType == MMTRAP) {
		// We can only specify the trap state vector once per trap type
		if (process->mm_trap_old_state == (state_t*)ENULL && process->mm_trap_new_state == (state_t*)ENULL) {
			process->mm_trap_old_state = old_state_area;
			process->mm_trap_new_state = new_state_area;
		} 
		else {
			killproc();
		}
	}
	else if (trapType == SYSTRAP) {
		// We can only specify the trap state vector once per trap type
		if (process->sys_trap_old_state == (state_t*)ENULL && process->sys_trap_new_state == (state_t*)ENULL) {
			process->sys_trap_old_state = old_state_area;
			process->sys_trap_new_state = new_state_area;
		} 
		else {
			killproc();
		}
	}

	// Update the processor state by setting the proc_t state ps to the OLD SYS PROCESS STATE area
	process->p_s = *SYS_TRAP_OLD_STATE;

	// Call schedule to exit this kernel routine and switch the execution flow back the interrupted process
	schedule();
}


/*
	When this instruction is executed, it causes the CPU time (in microseconds) used by
	the process executing the instruction to be placed in D2. This means that the
	nucleus must record (in the process table entry) the amount of processor time used by each process.
*/
void getcputime()
{
	// The interrupted process's state is saved in SYS_TRAP_OLD_STATE
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process
	proc_t* process = headQueue(readyQueue);

	// Get the time spent on the CPU from the process
	SYS_TRAP_OLD_STATE->s_r[2] = process->total_processor_time;

	// Update the processor state by setting the proc_t state ps to the OLD SYS PROCESS STATE area
	process->p_s = *SYS_TRAP_OLD_STATE;

	// Call schedule to exit this kernel routine and switch the execution flow back the interrupted process
	schedule();
}


/*
	Handles all other SYS traps.
*/
void trapsysdefault()
{
	// The interrupted process's state is saved in SYS_TRAP_OLD_STATE
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process from the RQ
	proc_t* process = headQueue(readyQueue);

	if (process->sys_trap_old_state != (state_t*)ENULL && process->sys_trap_new_state != (state_t*)ENULL) {
		// Copy the interrupted process state (stored in 0x930) into the process's SYS Trap Old State Area
		*process->sys_trap_old_state = *SYS_TRAP_OLD_STATE;

		// Load the Handler State routine specifics stored in this process's SYS New State struct ptr (address set in SYS5) onto the CPU
		LDST(&process->sys_trap_new_state);
	}
	else {
		// No handler address in the PTE for this trap, kill the process at the head of RQ
		killproc();
	}
}
