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
	*((int*)0x008) = (int)STLDMM;
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
	PROG_TRAP_NEW_STATE->s_sr.ps_int = 7; 				      // All interrupts disabled for process trap handler
	PROG_TRAP_NEW_STATE->s_sr.ps_m = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	PROG_TRAP_NEW_STATE->s_sr.ps_s = 1;   				      // Switch to Supervisor Mode
	PROG_TRAP_NEW_STATE->s_sp = MEMSTART;					  // Set the global stack to the top, where the Kernel memory chunk is allocated
	PROG_TRAP_NEW_STATE->s_pc = (int)trapproghandler;	      // The address for this specific handler

	// Allocate New and Old State Areas for Memory Management Traps
	MM_TRAP_OLD_STATE = (state_t*)0x898;					  // Set pointer to address in Memory -> 76 bytes
	state_t* MM_TRAP_NEW_STATE = MM_TRAP_OLD_STATE + 1;       // Offset for New State area
	MM_TRAP_NEW_STATE->s_sr.ps_int = 7; 					  // All interrupts disabled for mm trap handler
	MM_TRAP_NEW_STATE->s_sr.ps_m = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	MM_TRAP_NEW_STATE->s_sr.ps_s = 1;   					  // Switch to Supervisor Mode
	MM_TRAP_NEW_STATE->s_sp = MEMSTART;					      // Set the global stack to the top, where the Kernel memory chunk is allocated
	MM_TRAP_NEW_STATE->s_pc = (int)trapmmhandler;	          // The address for this specific handler

	// Allocate New and Old State Trap Areas for SYS Traps
	SYS_TRAP_OLD_STATE = (state_t*)0x930;					  // Set pointer to address in Memory -> 76 bytes
	state_t* SYS_TRAP_NEW_STATE = SYS_TRAP_OLD_STATE + 1;	  // Offset for New State area
	SYS_TRAP_NEW_STATE->s_sr.ps_int = 7; 				      // All interrupts disabled for mm trap handler
	SYS_TRAP_NEW_STATE->s_sr.ps_m = 0;	 				      // Set memory management to physical addressing (no process virutalization)
	SYS_TRAP_NEW_STATE->s_sr.ps_s = 1;   				      // Switch to Supervisor Mode
	SYS_TRAP_NEW_STATE->s_sp = MEMSTART;					  // Set the global stack to the top, where the Kernel memory chunk is allocated
	SYS_TRAP_NEW_STATE->s_pc = (int)trapsyshandler;		      // The address for this specific handler
}


/*
	When a trap/exception occurs, the hardware auto-saves the interrupted process state to the designated trap's old state area (0x800, 0x900, 0x930)

	WHEN a TRAP HANDLER IS INVOKED WE CHECK THAT SYS5 (Specify_Trap_Vector) HAS BEEN 
	PROPERLY INVOKED BEFOREHAND AND THAT IT HAS ALREADY SPECIFCIED THE CORRECT TRAP HANDLER (state_t* xx_trap_new_state) 
	IN THE INTERRUPTED PROC STRUCT AND THE ADDRESS TO STORE THE OLD/INTERRUPTED PROC STATE (state_t* xx_trap_old_state, that addr points elsewhere)

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
	if (SYS_TRAP_OLD_STATE->s_sr.ps_s != 1 && SYS_TRAP_OLD_STATE->s_tmp.tmp_sys.sys_no < 9) {

		// Update the system trap old state struct -> prog trap type
		SYS_TRAP_OLD_STATE->s_tmp.tmp_pr.pr_typ = PRIVILEGE;

		if (process->prog_trap_old_state != (state_t*)ENULL && process->prog_trap_new_state != (state_t*)ENULL) {
			// Copy the interrupted process state (stored in 0x800) into the process's Prog Trap Old State Area
			*process->prog_trap_old_state = *PROG_TRAP_OLD_STATE;

			// Load the Handler State routine specifics stored in this process's New State struct ptr (address set in SYS5) onto the CPU
			LDST(&process->prog_trap_new_state);
		} 
		else {
			// No handler address in the PTE for this trap, kill the interrupted process at the head of RQ
			killproc();
		}
	}

	// For traps that require SYS calls 1- 6, we only need to use the Process's SYS old trap state struct to determine the exact handler needed
	switch (SYS_TRAP_OLD_STATE->s_tmp.tmp_sys.sys_no) {
		case (1):
			createproc();
			break;
		case (2):
			killproc();
			break;
		case (3):
			semop();
			break;
		case (4):
			notused();
			break;
		case (5):
			trapstate();
			break;
		case (6):
			getcputime();
			break;
		default:
			if (process->sys_trap_old_state != (state_t*)ENULL && process->sys_trap_new_state != (state_t*)ENULL) {
				// Copy the interrupted process state (stored in 0x930) into the process's SYS Trap Old State Area
				*process->sys_trap_old_state = *SYS_TRAP_OLD_STATE;

				// Load the Handler State routine specifics stored in this process's New State struct ptr (address set in SYS5) onto the CPU
				LDST(&process->sys_trap_new_state);
			}
			else {
				// No handler address in the PTE for this trap, kill the process at the head of RQ
				killproc();
			}
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

	if (process->mm_trap_old_state != (state_t*)ENULL && process->mm_trap_new_state != (state_t*)ENULL) {
		// Copy the interrupted process state (stored in 0x898) into the process's MM Trap Old State Area
		*process->mm_trap_old_state = *MM_TRAP_OLD_STATE;

		// Load the Handler State routine specifics stored in this process's New State struct ptr (address set in SYS5) onto the CPU
		LDST(&process->mm_trap_new_state);
	}
	else {
		// No handler address in the PTE for this trap, kill the process at the head of RQ
		killproc();
	}
}


/*
	Pass up Memory Managment trap or terminate the process
	WE'RE KEEPING PROCESSES AT THE HEAD OF THE QUEUE WHILE THEY EXECUTE, ONLY REMOVING THEM WHEN THEY TERMINATE
	NOTE: PROCESSES THAT FINISH OR ARE BLOCKED BY A SEMAPHORE ARE REMOVED FROM TEH RQ
*/
void static trapproghandler()
{
	// Grab the interrupted process from the Ready Queue
	proc_t* process = headQueue(ready_queue);

	if (process->prog_trap_old_state != (state_t*)ENULL && process->prog_trap_new_state != (state_t*)ENULL) {
		// Copy the interrupted process state (stored in 0x800) into the process's Prog Trap Old State Area
		*process->prog_trap_old_state = *PROG_TRAP_OLD_STATE;

		// Load the Handler State routine specifics stored in this process's New State struct ptr (address set in SYS5) onto the CPU
		LDST(&process->prog_trap_new_state);
	} 
	else {
		// No handler address in the PTE for this trap, kill the process at the head of RQ
		killproc();
	}
}

