/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
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
void killproc();


void createproc()
{
	// Get the interrupted processor state via SYS_OLD_STATE_AREA
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process from the RQ, serving as the parent process
	proc_t* parentProcess = headQueue(readyQueue);

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

		// Add the child process to the tail of RQ
		insertProc(&readyQueue, childProcess);
	}
}


// Recursive function to terminate process and its children
void killprocrecurse(proc_t* process)
{
	// Base case
	if (process == (proc_t*)ENULL) {
		return;
	}

	// DFS into the last child, then explore the children of this prcoess's siblings
	proc_t* childProcess = process->children_proc;
    while (childProcess != (proc_t*)ENULL) {
		proc_t* nextChildProcess = childProcess->sibling_proc;
		killprocrecurse(childProcess);
		childProcess = nextChildProcess;
	}

	// Backtrack and remove all of the process's progeny links and from ASL queues
	if (outBlocked(process) == (proc_t*)ENULL) {
		outProc(&readyQueue, process);
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
	// Grab the interrupted process from the RQ, serving as the parent process
	proc_t* process = headQueue(readyQueue);

	// Set the parent child pointer to the killed process's immeadiate sibling
	proc_t* parentProcess = process->parent_proc;

	if (parentProcess != (proc_t*)ENULL) {
		// if the parent process child pointer starts with this process, make parent's first child this process's sibling
		if (parentProcess->children_proc == process) {
			parentProcess->children_proc = process->sibling_proc;
		}
		else {
			// Otherwise find the sibling previous to the killed process and update its sibling pointer to the kill process's sibling
			proc_t* child = parentProcess->children_proc;

			while (child != (proc_t*)ENULL && child->sibling_proc != process) {
				child = child->sibling_proc;
			}

			if (child != (proc_t*)ENULL) {
				child->sibling_proc = process->sibling_proc;
			}
		}
	}

	// Kill family tree and remove this process from the RQ
	removeProc(&readyQueue);
	killprocrecurse(process);

	// Call schedule to exit this kernel routine, prime the IT, and load the next process on the RQ
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
	// Get the interrupted processor state via SYS_OLD_STATE_AREA
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Get vpop struct and length
	int callingProcessBlocked = FALSE;
	int len = SYS_TRAP_OLD_STATE->s_r[3];
	vpop* semOperations = (vpop*)SYS_TRAP_OLD_STATE->s_r[4];

	// Iterate thorugh each entry and peform action on semaphore with given address based on the operation type
	int i;
	for (i = 0; i < len; i++) {
		int op = semOperations[i].op;			// Get operation to be performed on semaphore
		int* semAddr = semOperations[i].sem;	// Get the semaphore address
		int prevSemVal = *semAddr;				// Get the semaphore proper
		*semAddr = prevSemVal + op;				// Update the semaphore

		// P (-1) will decrement the semaphore, if the sem value is negative afterwards, the interrupted process should be blocked
		if (op == LOCK) {
			if (prevSemVal <= 0) {
				// Semaphore has become negative, meaning its blocking at least the process and is now active
				// The running process at the head of the Queue can be blocked by a P operation 
				// we do not want this to prevent other processes from being unblocked, so use a flag
				callingProcessBlocked = TRUE;
				proc_t* callingProcess = headQueue(readyQueue);
				callingProcess->p_s = *SYS_TRAP_OLD_STATE;
				insertBlocked(semAddr, callingProcess);
			}
			else {
				// Do nothing if the semaphore still has resources
			}
		}
		// V (+1) on an active semaphore (-value) means a resource has been freed, allowing the next blocked process on that Semaphore to maybe be put back on RQ
		else if (op == UNLOCK) {
			if (prevSemVal < 0) {
				// Remove the process at the head of the corresponding Semaphore Queue and update Semvec
				proc_t* process = removeBlocked(semAddr);

				// If the process is no longer blocked on any Semaphores, then add it back to the RQ
				if (process != (proc_t*)ENULL && process->qcount == 0) {
					insertProc(&readyQueue, process);
				}
			}
			else {
				// Do nothing if the semaphore was not active, indicative of available resources
			}
		}
	}

	// Ensure atomic operation, even if calling process was blocked
	if (callingProcessBlocked) {
		removeProc(&readyQueue);
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
	  - The type of trap for which a trap state vector is being established. This information will be placed in D2 at the time of the call, using the following encoding:
		0 - program trap
		1 - memory management trap
		2 - SYS trap

	  - The area into which the processor state (the old state) is to be stored when a trap
	    occurs while running this process. THE ADDRESS OF THIS AREA WILL BE IN D3.

	  - The processor state area that is to be taken as the new processor state if a trap
	    occurs while running this process. THE ADDRESS OF THIS AREA WILL BE IN D4.

	The nucleus, on execution of this instruction, should save the contents of D3 and D4
	(in the process table entry) to pass up the appropriate trap when (and if) it occurs while running this process.
*/
void trapstate()
{
	// The interrupted process's state is saved in old_state (SYS)
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process so we populate the corresponding old processor state area with whats in 0x930
	proc_t* process = headQueue(readyQueue);

	// Hardware will have loaded the appropiate info in SYS_OLD_STATE_AREA
	int trapType = SYS_TRAP_OLD_STATE->s_r[2];
	state_t* old_state_area = (state_t*)SYS_TRAP_OLD_STATE->s_r[3];	  // address to the old state area we will populate later with the old processor state
	state_t* new_state_area = (state_t*)SYS_TRAP_OLD_STATE->s_r[4];   // address to the specific handler processor state for this trap, already populated!

	// Make sure the corresponding pointers in proc_t have not been popoulated (SYS5 was not already invoked)
	switch (trapType) {
		case (PROGTRAP):
			// We can only specify the trap state vector once per trap type
			if (process->prog_trap_old_state == (state_t*)ENULL && process->prog_trap_new_state == (state_t*)ENULL) {
				process->prog_trap_old_state = old_state_area;
				process->prog_trap_new_state = new_state_area;
				break;
			}
			else {
				killproc();
			}
		case (MMTRAP):
			// We can only specify the trap state vector once per trap type
			if (process->mm_trap_old_state == (state_t*)ENULL && process->mm_trap_new_state == (state_t*)ENULL) {
				process->mm_trap_old_state = old_state_area;
				process->mm_trap_new_state = new_state_area;
				break;
			} 
			else {
				killproc();
			}
		case (SYSTRAP):
			// We can only specify the trap state vector once per trap type
			if (process->sys_trap_old_state == (state_t*)ENULL && process->sys_trap_new_state == (state_t*)ENULL) {
				process->sys_trap_old_state = old_state_area;
				process->sys_trap_new_state = new_state_area;
				break;
			} 
			else {
				killproc();
			}
	}
}


/*
	When this instruction is executed, it causes the CPU time (in microseconds) used by the process executing the instruction to be placed in D2.
*/
void getcputime()
{
	// The interrupted process's state is saved in old_state (SYS)
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process
	proc_t* process = headQueue(readyQueue);

	// Get the time spent on the CPU from the process
	SYS_TRAP_OLD_STATE->s_r[2] = process->total_processor_time;
}


/*
	Handles all other SYS traps.
*/
void trapsysdefault()
{
	// The interrupted process's state is saved in old_state (SYS)
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Grab the interrupted process from the RQ
	proc_t* process = headQueue(readyQueue);

	// The process's old state area has been initialized and the appropiate new sys handler is present in the process's new sys area
	if (process->sys_trap_old_state != (state_t*)ENULL && process->sys_trap_new_state != (state_t*)ENULL) {
		// Update process start time as we load sys trap handler on CPU
		updateLastStartTime(process);

		// Copy the interrupted process state (stored in 0x930) into the process's SYS Trap Old State Area
		*process->sys_trap_old_state = *SYS_TRAP_OLD_STATE;

		// Load the Handler State routine specifics stored in this process's SYS New State struct ptr (address set in SYS5) onto the CPU
		LDST(process->sys_trap_new_state);
	}
	else {
		// No handler address in the PTE for this trap or area to store its previous state
		killproc(process);
	}
}
