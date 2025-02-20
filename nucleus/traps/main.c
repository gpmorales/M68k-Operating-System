/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../interrupts/int.c"
#include "../../h/procq.e"				
#include "../../h/trap.e"				
#include "../../h/asl.e"				
#include "stdio.h"

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

proc_link ready_queue;


void main() 
{
	// Determines physical memory in the system and initializes Active Sem List, Process Table, and Traps
	init();

	// Initialize the processor state area for p1()
	state_t global_state;

	// Store the initial processor state when the OS boots up to provide clean baseline for all other processes
	STST(&global_state); 

	// Allocate a Process entry for the initial process p1 from the Process free list
	proc_t* initial_proc = allocProc();


	// *********** TODO ASK IF THIS IS NECESSARY AND IF INITIALIZING P1 PROCESSOR STATE DIRECTLY IS BETTER ???? ***********


	// Since we initalized a valid process state already, we can set aside another state area for p1 
	state_t* p1_state = &initial_proc->p_s;
	STST(&p1_state);

	// Update the stack pointer and the program counter for p1's processor state
	p1_state->s_sp = global_state.s_sp - PAGESIZE*2; // Move stack pointer down from the initial global process area
	p1_state->s_pc = (int)p1;

	// Now set the status register flags for p1 process state
	p1_state->s_sr.ps_s = 0;	// Set memory management to physical addressing (no process virutalization)
	p1_state->s_sr.ps_m = 1;    // Switch to Supervisor Mode to run initial process
	p1_state->s_sr.ps_int = 7;  // All interrupts disabled for initial process p1

	// Insert the initial process into the RQ
	insertProc(&ready_queue, initial_proc);

	// Begin scheduling tasks for the CPU to execute form the Ready Queue
	schedule();
}


void static init()
{
	// Total physical memory on system (128K)
	ready_queue.index = ENULL;
	ready_queue.next = (proc_link*)ENULL;

	initProc(); // Initialize Process Free List
	initSemd(); // Initialize In-active Semaphore List
	trapinit(); // Initialize EVT + Sys, MM, and SYS Trap Areas
	intinit();  // Phase 2
}


void schedule()
{
	// Put process in RQ but do not remove it from the queue
	proc_t* next_proc;

	if ((next_proc = headQueue(ready_queue)) != (proc_t*)ENULL) {
		state_t next_proc_state = next_proc->p_s;
		// Load this process's state into the CPU? TODO ASK IF THIS IS CORRECTE DESC
		LDST(&next_proc_state);
		intschedule();
		return;
	} 

	// No process on RQ for the CPU to execute, trigger a trap with no handler and halt CPU
	intdeadlock();
}
