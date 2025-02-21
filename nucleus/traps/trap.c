/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../../h/syscall.e"				
#include "../../h/types.h"				
#include "../../h/const.h"				
#include "../../h/trap.e"
#include "../../h/main.e"				
#include "../../h/procq.e"				
#include "../../h/util.h"

/*
	This module handles the traps, it has the following static functions:
	trapinit(), trap-syshandler(), trapmmhandler(), and trapproghandler()

		- void trapinit(): 
		Populates the Exception Vector Table entries and allocates memory for each exception handler, storing their addresses in the EVT.
		When traps occur, the control flow is transferred via the Trap State Vector which saves the old processor state and 
		loads the new processor state's address from the EVT.

		- void static trapsyshandler():
		This function handles 9 different traps. It has a switch statment and each case calls a function.
		Two of the functions, waitforpclock() and waitforio() are in int.c The other secen are in syscall.c

		NOTE: During init() I will map EVT trap numbers (32-47) to their corresponding SYS functions. 
		The tmp_sys.sys_no field will hold that trap number, letting the kernel know which SYS function (SYS1-SYS7) to execute.

		- void static trapmmhandler():
		- void static trapproghandler():
		These functions will pass up memory management and program traps OR terminate the process.
*/

state_t* prog_trap_old_state;
state_t* sys_trap_old_state;
state_t* mm_trap_old_state;

void trapinit()
{
	// Populate EVT with function addresses
	*((int*)0x014) = (int)STLDMM;
	*((int*)0x00c) = (int)STLDADDRESS();		   
	*((int*)0x010) = (int)STLDILLEGAL();		   
	*((int*)0x014) = (int)STLDZERO();			   
	*((int*)0x020) = (int)STLDPRIVILEGE();		   
	*((int*)0x08c) = (int)STLDSYS();			   
	*((int*)0x94 ) = (int)STLDSYS9();			   
	*((int*)0x98 ) = (int)STLDSYS10();			   
	*((int*)0x9c ) = (int)STLDSYS11();			   
	*((int*)0xa0 ) = (int)STLDSYS12();			   
	*((int*)0xa4 ) = (int)STLDSYS13();			   
	*((int*)0xa8 ) = (int)STLDSYS14();			   
	*((int*)0xac ) = (int)STLDSYS15();			   
	*((int*)0xb0 ) = (int)STLDSYS16();			   
	*((int*)0xb4 ) = (int)STLDSYS17();			   
	*((int*)0x100) = (int)STLDTERM0();			   
	*((int*)0x104) = (int)STLDTERM1();			   
	*((int*)0x108) = (int)STLDTERM2();			   
	*((int*)0x10c) = (int)STLDTERM3();			   
	*((int*)0x110) = (int)STLDTERM4();			   
	*((int*)0x114) = (int)STLDPRINT0();		    
	*((int*)0x11c) = (int)STLDDISK0();
	*((int*)0x12c) = (int)STLDFLOPPY0();
	*((int*)0x140) = (int)STLDCLOCK();

	// Allocate New and Old State Areas for Program Traps
	prog_trap_old_state = (state_t*)0x800;					  // 76 bytes
	state_t* prog_trap_new_state = prog_trap_old_state + 1;   // Offset for New State area
	STST(&prog_trap_old_state);								  // Initialize the Old State area at startup by populating the 76-byte state_t struct used for prog trap handling
	prog_trap_new_state->s_sr.ps_int = 7; 				      // All interrupts disabled for process trap handler
	prog_trap_new_state->s_sr.ps_s = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	prog_trap_new_state->s_sr.ps_m = 1;   				      // Switch to Supervisor Mode
	prog_trap_new_state->s_pc = (int)trapproghandler;	      // The address for this specific handler

	// Allocate New and Old State Areas for Memory Management Traps
	mm_trap_old_state = (state_t*)0x898;					  // 76 bytes
	state_t* mm_trap_new_state = mm_trap_old_state + 1;       // Offset for New State area
	STST(&mm_trap_old_state);								  // Initialize the Old State area at startup by populating the 76-byte state_t struct used for mm trap handling
	mm_trap_new_state->s_sr.ps_int = 7; 					  // All interrupts disabled for mm trap handler
	mm_trap_new_state->s_sr.ps_s = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	mm_trap_new_state->s_sr.ps_m = 1;   					  // Switch to Supervisor Mode
	mm_trap_new_state->s_pc = (int)trapmmhandler;	          // The address for this specific handler

	// Allocate New and Old State Trap Areas for SYS Traps
	sys_trap_old_state = (state_t*)0x930;					  // 76 bytes
	state_t* sys_trap_new_state = sys_trap_old_state + 1;	  // Offset for New State area
	STST(&sys_trap_old_state);								  // Initialize the Old State area at startup by populating the 76-byte state_t struct used for sys trap handling
	sys_trap_new_state->s_sr.ps_int = 7; 				      // All interrupts disabled for mm trap handler
	sys_trap_new_state->s_sr.ps_s = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	sys_trap_new_state->s_sr.ps_m = 1;   				      // Switch to Supervisor Mode
	sys_trap_new_state->s_pc = (int)trapsyshandler;		      // The address for this specific handler
}


/*
	Note that only instructions SYS1 to SYS8 are treated as instructions by the nucleus.
	The other SYS instructions(SYS9 to SYS17) are passed up as SYS traps
*/
void static trapsyshandler() 
{
	state_t* current_state;

	// Capture the current processor state
	STST(&current_state); 

	// Verify that the invoking process is in supervisor mode and is a SYS call
	if (current_state->s_sr.ps_m != 1 && current_state->s_tmp.tmp_sys.sys_no < 9) {
		// Save the new state in the Program Trap's New Area State
		state_t* privilege_violation_state = prog_trap_old_state + 1;

		// Save the process state in the Program Trap's Old Area State
		*prog_trap_old_state = *current_state;

		privilege_violation_state->s_sr.ps_int = 7; 				// All interrupts disabled for mm trap handler
		privilege_violation_state->s_sr.ps_s = 0;	 				// Set memory management to physical addressing (no process virutalization)
		privilege_violation_state->s_sr.ps_m = 1;   				// Switch to Supervisor Mode to run initial proc
		privilege_violation_state->s_sp = ((int)0x0020);			// Set PC to the STLDPRIVILEGE function address

		// Trigger the privilege violation trap and invoke handler (does not return)
		LDST(&privilege_violation_state);
	}

	// Save the process state in the System Trap's Old Area State
	*sys_trap_old_state = *current_state;

	switch (current_state->s_tmp.tmp_sys.sys_no) {
		case (1):
			createproc(sys_trap_old_state);
			break;
		case (2):
			killproc(sys_trap_old_state);
			break;
		case (3):
			semop(sys_trap_old_state);
			break;
		case (4):
			notused(sys_trap_old_state);
			break;
		case (5):
			trapstate(sys_trap_old_state);
			break;
		case (6):
			getcputime(sys_trap_old_state);
			break;
		default:
			// Use the System Trap number to find the corresponding Handler Routine address to handle this case
			pass_up_sys_trap(sys_trap_old_state);
			break;
	}

	// Return flow to original process since these are legitmate kernel service request
	LDST(&sys_trap_old_state);
}



/*
	Handle Memory Managment traps
*/
void static trapmmhandler() 
{
	// Capture the current processor state
	state_t* current_state;
	STST(&current_state); 

	// Save the current program state in MM Old Trap Area
	*mm_trap_old_state = *current_state;

	// Prepare the new state
	state_t* mm_trap_new_state = mm_trap_old_state + 1;
	mm_trap_new_state->s_sr.ps_int = 7; 				// All interrupts disabled for mm trap handler
	mm_trap_new_state->s_sr.ps_s = 0;	 				// Set memory management to physical addressing (no process virutalization)
	mm_trap_new_state->s_sr.ps_m = 1;   				// Switch to Supervisor Mode to run initial proc
	int mm_trap_handler = ((int)4 * current_state->s_tmp.tmp_pr.pr_typ);
	mm_trap_new_state->s_pc = mm_trap_handler;

	// Invoke mm trap handler
	LDST(&mm_trap_new_state);
}


/*
	Pass up Memory Managment trap or terminate the process

	TODO:
	ARE we're keeping processes at the head of the queue while they execute,
	only removing them when they terminate?
*/
void static trapproghandler()
{
	// Grab the next avail process from the Ready Queue
	proc_t* process = headQueue(ready_queue);

	// Kill current process if CPU is starved
	if (process == (proc_t*)ENULL) {
		killproc(prog_trap_old_state);
	}

	// Save the process's program state in Program Trap's Old State Area ?????????????????
	*prog_trap_old_state = process->p_s;

	// Load the new state and invoke the stored handler specifics for this process via its new state address (set in SYS5)
	LDST(&process->prog_trap_new_state);

	/* Alternative:
		STST(&current_state);
		*prog_trap_old_state = *current_state;

		state_t* prog_trap_new_state = prog_trap_old_state + 1;
		prog_trap_new_state->s_sr.ps_int = 7; 				// All interrupts disabled for mm trap handler
		prog_trap_new_state->s_sr.ps_s = 0;	 				// Set memory management to physical addressing (no process virutalization)
		prog_trap_new_state->s_sr.ps_m = 1;   				// Switch to Supervisor Mode to run initial proc
		int prog_trap_handler = ((int)4 * process.state_t.s_tmp.tmp_pr.pr_typ);
		prog_trap_new_state->s_pc = prog_trap_handler;
		LDST(prog_trap_new_state);
	*/
}


