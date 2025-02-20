/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/

#include "../../h/syscall.e"				
#include "../../h/types.h"				
#include "../../h/const.h"				
#include "../../h/trap.e"
#include "../../h/util.h"
#include "stdio.h"

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
	state_t* prog_old_state = (state_t*)0x800;		// 76 bytes
	state_t* prog_new_state = prog_old_state + 1;   // Offset for New State area
	prog_new_state->s_sr.ps_int = 7; 				// All interrupts disabled for process trap handler
	prog_new_state->s_sr.ps_s = 0;	 				// Set memory management to physical addressing (no process virutalization)
	prog_new_state->s_sr.ps_m = 1;   				// Switch to Supervisor Mode to run initial proc
	// TODO
	prog_new_state->s_pc = NULL;	 				// The address for this specific handler

	// Allocate New and Old State Areas for Memory Management Traps
	state_t* mm_old_state = (state_t*)0x898;	    // 76 bytes
	state_t* mm_new_state = mm_old_state + 1;       // Offset for New State area
	prog_new_state->s_sr.ps_int = 7; 				// All interrupts disabled for mm trap handler
	prog_new_state->s_sr.ps_s = 0;	 				// Set memory management to physical addressing (no process virutalization)
	prog_new_state->s_sr.ps_m = 1;   				// Switch to Supervisor Mode to run initial proc
	// TODO
	mm_new_state->s_pc = NULL;						// The address for this specific handler

	// Allocate New and Old State Trap Areas for SYS call traps
	state_t* sys_old_state = (state_t*)0x930;		// 76 bytes
	state_t* sys_new_state = sys_old_state + 1;		// Offset for New State area
	sys_new_state->s_sr.ps_int = 7; 				// All interrupts disabled for mm trap handler
	sys_new_state->s_sr.ps_s = 0;	 				// Set memory management to physical addressing (no process virutalization)
	sys_new_state->s_sr.ps_m = 1;   				// Switch to Supervisor Mode to run initial proc
	// TODO
	sys_new_state->s_pc = NULL; // The address for this specific handler
}


// This function handles 9 different traps. It has a switch statment and each case calls a function.
// Two of the functions, waitforpclock() and waitforio() are in int.c The other seven are in syscall.c
void static trapsyshandler() 
{
	state_t* current_state;

	// Capture the current processor state when handling the 
	STST(&current_state); 

	// Verify that the invoking process is in supervisor mode
	if (current_state->s_sr.ps_m == 0) {
		// Get Program Trap Area address
		state_t* program_trap_old_state = (state_t*)0x800; 

		// Save the new state in the Program Trap's New Area State
		state_t* privilege_violation_state = program_trap_old_state + 1;

		// Save the process state in the Program Trap's Old Area State
		// the content for which the old state var holds should be the current state data
		*program_trap_old_state = *current_state;

		privilege_violation_state->s_sr.ps_int = 7; 				// All interrupts disabled for mm trap handler
		privilege_violation_state->s_sr.ps_s = 0;	 				// Set memory management to physical addressing (no process virutalization)
		privilege_violation_state->s_sr.ps_m = 1;   				// Switch to Supervisor Mode to run initial proc
		privilege_violation_state->s_sp = ((int)0x0020);			// Set PC to the STLDPRIVILEGE function address

		// Trigger the privilege violation trap and invoke corresponding handler on LDST
		LDST(&privilege_violation_state);
	}

	// If in supervisor mode, call proper SYS handler function 

	// Get System Trap Area address
	state_t* system_trap_old_state = (state_t*)0x930; 

	// Save the process state in the System Trap's Old Area State
	*system_trap_old_state = *current_state;

	switch (current_state->s_tmp.tmp_sys.sys_no) {
		case (1):
			createproc(system_trap_old_state);
			break;
		case (2):
			killproc(system_trap_old_state);
			break;
		case (3):
			semop(system_trap_old_state);
			break;
		case (4):
			notused(system_trap_old_state);
			break;
		case (5):
			trapstate(system_trap_old_state);
			break;
		case (6):
			getcputime(system_trap_old_state);
			break;
	}

}


