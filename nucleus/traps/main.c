/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../interrupts/int.c"
#include "../../h/procq.e"				
#include "../../h/trap.e"				
#include "../../h/asl.e"				

extern int p1();

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
proc_link ready_queue;

void main() 
{
	// SETS ASIDE A CHUNK OF MEOMRY FOR THE KERNEL ROUTINES AT THE LARGEST MEMORY LOCATION 
	init();

	// Allocate a Process entry for the initial process p1 from the Process free list
	proc_t* initial_proc = allocProc();

	// Prepare the processor state in the CPU for p1. Populate some registers, SP, and PC
	state_t* p1_state;

	// Update the stack pointer and move it down from the Kernel chunk memory to prevent overriding Kernel routines
	p1_state->s_sp = MEMSTART - PAGESIZE*2; 
	p1_state->s_sr.ps_m = 0;					// Set memory management to physical addressing (no process virutalization)
	p1_state->s_sr.ps_s = 1;					// Switch to Supervisor Mode to run initial process
	p1_state->s_sr.ps_int = 7;					// All interrupts disabled for initial process p1
	p1_state->s_pc = (int)p1;					// Set the Program Counter to p1's address
	initial_proc->p_s = *p1_state;				// Update proc_t with the the current processor state

	// Insert the initial process into the RQ
	insertProc(&ready_queue, initial_proc);

	// Begin scheduling tasks for the CPU to execute form the Ready Queue
	schedule();
}


void static init()
{
	// Calculate physical memory on system via the stack pointer on boot-up 

	// Initialize the processor state area for p1()
	state_t global_state;

	// Store the initial processor state when the OS boots up to provide clean baseline for all other processes
	STST(&global_state); 

	// Grab the global stack pointer which is at the top of memory
	MEMSTART = global_state.s_sp;

	// Prep RQ
	ready_queue.index = ENULL;
	ready_queue.next = (proc_link*)ENULL;

	initProc(); // Initialize Process Free List
	initSemd(); // Initialize In-active Semaphore List
	trapinit(); // Initialize EVT + Prog, MM, and SYS Trap Areas
	intinit();  // Phase 2
}


void schedule()
{
	// Put process in RQ but do not remove it from the queue
	proc_t* ready_proc;

	if ((ready_proc = headQueue(ready_queue)) != (proc_t*)ENULL) {
		state_t this_proc_state = ready_proc->p_s;
		// Load this process's state into the CPU
		LDST(&this_proc_state);
		intschedule();
	} 
	else {
		// CPU is starved -> trigger a trap with no handler and halt the CPU
		intdeadlock();
	}
}
