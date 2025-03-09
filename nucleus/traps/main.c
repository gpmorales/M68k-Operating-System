/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../../h/types.h"
#include "../../h/const.h"
#include "../../h/procq.e"
#include "../../h/asl.e"

/*
	This module coordinates the initialization of the nucleus and it starts
	the execution of the first process, p1(). It also provides a scheduling
	function. The module contains three routines:

	- void main()
	This function calls init(), sets up the processor state [state_t] for p1(),
	adds p1() to the Ready Queue and calls the schedule() routine

	- void static init():
	This function determines how much phyiscal memory there is in the system. It
	then calls initProc(), initSemd(), trapinit(), and intinit()

	- void schedule()
	if the RQ is not empty this function calls intschedule() and loads the state of
	the process at the head of the RQ. If the RQ is empty it calls intdeadlock().
*/

int MEMSTART;
proc_link readyQueue;

extern int p1();
extern void updateLastStartTime(proc_t* p);


void static init()
{
	// Initialize the processor state area for p1()
	state_t globalState;

	// Store the initial processor state when the OS boots up to provide clean baseline for all other processes
	STST(&globalState); 

	// Grab the global stack pointer which is at the top of memory
	MEMSTART = globalState.s_sp;

	// Prep RQ
	readyQueue.index = ENULL;
	readyQueue.next = (proc_t*)ENULL;

	initProc(); // Initialize Process Free List
	initSemd(); // Initialize In-active Semaphore List
	trapinit(); // Initialize EVT + Prog, MM, and SYS Trap Areas
	intinit();  // Initialize Interrupt Areas and Device registers
}


// After the Kernel routines handle the trap or interrupt, we call schedule to resume the exeuction of the old process if applicable
void schedule()
{
	// Prepare to run next process in RQ
	proc_t* readyProcess;

	if ((readyProcess = headQueue(readyQueue)) != (proc_t*)ENULL) {
		state_t state = readyProcess->p_s;
		// Load this process's state into the CPU
		intschedule();
		updateLastStartTime(readyProcess);
		LDST(&state);
	} 
	else {
		// CPU is starved -> trigger a trap with no handler and halt the CPU
		intdeadlock();
	}
}


void main() 
{
	// SETS ASIDE A CHUNK OF MEOMRY FOR THE KERNEL ROUTINES AT THE LARGEST MEMORY LOCATION 
	init();

	// Allocate a Process entry for the initial process p1 from the Process free list
	proc_t* initialProcess = allocProc();

	// Prepare the processor state in the CPU for p1. Populate some registers, SP, and PC
	state_t initialProcState;

	// Update the stack pointer and move it down from the Kernel chunk memory to prevent overriding Kernel routines
	initialProcState.s_sp = MEMSTART - PAGESIZE*2; 
	initialProcState.s_sr.ps_m = 0;						// Set memory management to physical addressing (no process virutalization)
	initialProcState.s_sr.ps_s = 1;						// Switch to Supervisor Mode to run initial process
	initialProcState.s_sr.ps_int = 7;					// All interrupts disabled for initial process p1
	initialProcState.s_pc = (int)p1;					// Set the Program Counter to p1's address
	initialProcess->p_s = initialProcState;				// Update proc_t with the the current processor state

	// Insert the initial process into the RQ
	insertProc(&readyQueue, initialProcess);

	// Begin scheduling tasks for the CPU to execute form the Ready Queue
	schedule();
}
