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

state_t* PROG_TRAP_OLD_STATE;
state_t* SYS_TRAP_OLD_STATE;
state_t* MM_TRAP_OLD_STATE;

void trapinit()
{
	// Populate EVT with function addresses (Physical addresses from 0 to 0x800)
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
	PROG_TRAP_OLD_STATE = (state_t*)0x800;					  // Set pointer to address in Memory -> 76 bytes
	state_t* PROG_TRAP_NEW_STATE = PROG_TRAP_OLD_STATE + 1;   // Offset for New State area
	STST(&PROG_TRAP_OLD_STATE);								  // Initialize the Old State area on startup by populating the 76-byte state_t struct used for prog trap handling
	PROG_TRAP_NEW_STATE->s_sr.ps_int = 7; 				      // All interrupts disabled for process trap handler
	PROG_TRAP_NEW_STATE->s_sr.ps_s = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	PROG_TRAP_NEW_STATE->s_sr.ps_m = 1;   				      // Switch to Supervisor Mode
	PROG_TRAP_NEW_STATE->s_pc = (int)trapproghandler;	      // The address for this specific handler

	// Allocate New and Old State Areas for Memory Management Traps
	MM_TRAP_OLD_STATE = (state_t*)0x898;					  // Set pointer to address in Memory -> 76 bytes
	state_t* MM_TRAP_NEW_STATE = MM_TRAP_OLD_STATE + 1;       // Offset for New State area
	STST(&MM_TRAP_OLD_STATE);								  // Initialize the Old State area on startup by populating the 76-byte state_t struct used for mm trap handling
	MM_TRAP_NEW_STATE->s_sr.ps_int = 7; 					  // All interrupts disabled for mm trap handler
	MM_TRAP_NEW_STATE->s_sr.ps_s = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	MM_TRAP_NEW_STATE->s_sr.ps_m = 1;   					  // Switch to Supervisor Mode
	MM_TRAP_NEW_STATE->s_pc = (int)trapmmhandler;	          // The address for this specific handler

	// Allocate New and Old State Trap Areas for SYS Traps
	SYS_TRAP_OLD_STATE = (state_t*)0x930;					  // Set pointer to address in Memory -> 76 bytes
	state_t* SYS_TRAP_NEW_STATE = SYS_TRAP_OLD_STATE + 1;	  // Offset for New State area
	STST(&SYS_TRAP_OLD_STATE);								  // Initialize the Old State area on startup by populating the 76-byte state_t struct used for sys trap handling
	SYS_TRAP_NEW_STATE->s_sr.ps_int = 7; 				      // All interrupts disabled for mm trap handler
	SYS_TRAP_NEW_STATE->s_sr.ps_s = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	SYS_TRAP_NEW_STATE->s_sr.ps_m = 1;   				      // Switch to Supervisor Mode
	SYS_TRAP_NEW_STATE->s_pc = (int)trapsyshandler;		      // The address for this specific handler
}


/*
	When a trap/exception occurs, the hardware auto-saves the interrupted process state to the designated trap's old state area (0x800, 0x900, 0x930)
	XX_TRAP_OLD_STATE -> state of the process when it causes Trap of type XX
	XX_TRAP_NEW_STATE -> state of the handler process and its specifics to handler Trap of type XX

	NOTE that only instructions SYS1 to SYS8 are treated as instructions by the nucleus (We define these routines).
	The other SYS instructions(SYS9 to SYS17) are passed up as SYS traps.
*/
void static trapsyshandler() 
{
	// Grab the next avail process from the Ready Queue
	proc_t* process = headQueue(ready_queue);

	// *** Recall that on interruption due to a trap, the Hardware saves the CPUs processor state in the Global Trap Areas ***

	// Case where that the invoking process is in NOT supervisor mode and is a SYS call we handle
	if (SYS_TRAP_OLD_STATE->s_sr.ps_m != 1 && SYS_TRAP_OLD_STATE->s_tmp.tmp_sys.sys_no < 9) {
		// No handler address in the PTE for this trap, kill the original process/state stored in PROG_TRAP_OLD_STATE
		if (process->prog_trap_new_state == (proc_t*)ENULL) {
			killproc(&PROG_TRAP_OLD_STATE);
		}

		// Update the system trap old state struct prog trap type
		SYS_TRAP_OLD_STATE->s_tmp.tmp_pr.pr_typ = PRIVILEGE;

		// Save the processor's interrupted state in the process's Program Trap Old Area State Struct
		*process->prog_trap_old_state = *PROG_TRAP_OLD_STATE;

		// Load the new state struct and handler specifics stored in this process via its new state struct ptr (address set in SYS5)
		LDST(&process->prog_trap_new_state);
	}

	// For traps that require SYS calls 1- 6, we only need to use the Process's SYS old trap state struct to determine the exact handler needed
	switch (SYS_TRAP_OLD_STATE->s_tmp.tmp_sys.sys_no) {
		case (1):
			createproc(SYS_TRAP_OLD_STATE);
			break;
		case (2):
			killproc(SYS_TRAP_OLD_STATE);
			break;
		case (3):
			semop(SYS_TRAP_OLD_STATE);
			break;
		case (4):
			notused(SYS_TRAP_OLD_STATE);
			break;
		case (5):
			trapstate(SYS_TRAP_OLD_STATE);
			break;
		case (6):
			getcputime(SYS_TRAP_OLD_STATE);
			break;
		default:
			// No handler address in the PTE for this trap, kill the original process/state stored in SYS_TRAP_OLD_STATE
			if (process->sys_trap_new_state == (state_t*)ENULL) {
				killproc(&SYS_TRAP_OLD_STATE);
			}
			// Copy the interrupted processor state struct captured on the SYS trap into the process's SYS-trap old state struct 
			*process->sys_trap_old_state = *SYS_TRAP_OLD_STATE;

			// Load the new process state and its handler specifics
			LDST(&process->sys_trap_new_state);
			break;
	}

	// In the case where we handled the SYS trap with one of our own routines, we return the flow to the 
	// original process whose state specific data is in the global sys_trap_old_state
	LDST(&SYS_TRAP_OLD_STATE);
}



/*
	Pass up Porgram trap or terminate the process
*/
void static trapmmhandler() 
{
	// Grab the next avail process from the Ready Queue
	proc_t* process = headQueue(ready_queue);

	if (process->mm_trap_new_state != (state_t*)ENULL) {
		// Copy the interrupted processor state struct captured on the MM trap into the process's MM-trap old state struct 
		*process->mm_trap_old_state = *MM_TRAP_OLD_STATE;

		// Load the new state struct and handler specifics stored in this process via its new state struct ptr (address set in SYS5)
		LDST(&process->mm_trap_new_state);
	} else {
		// No handler address in the PTE for this trap, kill the original process/state stored in MM_TRAP_OLD_STATE
		killproc(&MM_TRAP_OLD_STATE);
	}
}


/*
	Pass up Memory Managment trap or terminate the process
	TODO: ARE we're keeping processes at the head of the queue while they execute, only removing them when they terminate????????
*/
void static trapproghandler()
{
	// Grab the next avail process from the Ready Queue
	proc_t* process = headQueue(ready_queue);

	if (process->prog_trap_new_state != (state_t*)ENULL) {
		// Copy the interrupted processor state struct captured on the PROG trap into the process's PROG-trap old state struct 
		*process->prog_trap_old_state = *PROG_TRAP_OLD_STATE;

		// Load the new state struct and handler specifics stored in this process via its new state struct ptr (address set in SYS5)
		LDST(&process->prog_trap_new_state);
	} else {
		// No handler address in the PTE for this trap, kill the original process/state stored in PROG_TRAP_OLD_STATE
		killproc(&PROG_TRAP_OLD_STATE);
	}
}

