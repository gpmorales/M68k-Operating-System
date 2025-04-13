/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../../h/types.h"				
#include "../../h/const.h"				
#include "../../h/util.h"
#include "../../h/procq.e"				
#include "../../h/int.e"				


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

    NOTE: During init(), the EVT entries 32-47 will be mapped to the corresponding SYS functions addresses. 
    The tmp_sys.sys_no field will hold the appropiate trap number so the kernel can invoke the corresponding SYS routine (SYS1-SYS8)

    - void static trapmmhandler():
    - void static trapproghandler():

    These functions will pass up memory management and program traps OR terminate the process.
*/


/* Device related registers and semaphores */
extern int MEMSTART;
extern proc_link readyQueue;

/* Trap Area States */
state_t* PROG_TRAP_OLD_STATE;
state_t* SYS_TRAP_OLD_STATE;
state_t* MM_TRAP_OLD_STATE;

/* Trap Handlers */
void static trapsyshandler();
void static trapproghandler();
void static trapproghandler();

/* Utility time routines */
void updateTotalTimeOnProcessor(proc_t* p);
void updateLastStartTime(proc_t* p);


/*
    When a trap/exception occurs, the hardware auto-saves the interrupted process state to
    the designated trap's old state area (0x800, 0x900, 0x930)

	XX_TRAP_OLD_STATE -> state of the process when it threw a trap of type XX
	XX_TRAP_NEW_STATE -> trap handler process state along with registers, PC, SP, etc. needed to execute the handler routine

    Only SYS1-SYS8 are handled by routines defined in the nucleus. The other SYS routines (SYS9-SYS17) are passed up to trapsysdefault
*/
void static trapsyshandler() 
{
    // Grab the interrupted process from the RQ
    proc_t* process = headQueue(readyQueue);
    updateTotalTimeOnProcessor(process);

    // Case where that the invoking process is NOT in supervisor mode and is a SYS call we handle
    if (SYS_TRAP_OLD_STATE->s_sr.ps_s != 1 && SYS_TRAP_OLD_STATE->s_tmp.tmp_sys.sys_no < 9) {
        // Update the system trap old state struct -> prog trap type
        SYS_TRAP_OLD_STATE->s_tmp.tmp_pr.pr_typ = PRIVILEGE;

		// The process's old state area has been initialized and the appropiate new prog handler is present in the process's new prog area
        if (process->prog_trap_new_state != (state_t*)ENULL && process->prog_trap_old_state != (state_t*)ENULL) {
            // Update process start time as we load it unto the CPU
            updateLastStartTime(process);

            // Copy the interrupted process state (stored in 0x800) into the process's Prog Trap Old State Area
            *process->prog_trap_old_state = *SYS_TRAP_OLD_STATE;

            // Load the Handler State routine specifics stored in this process's New State struct ptr (address set in SYS5) onto the CPU
            LDST(process->prog_trap_new_state);
        } 
        else {
			// No handler address the PTE for this trap or area to store its previous state
            killproc();
        }
    }

    // Determine the exact system routine needed to handle trap
    switch (SYS_TRAP_OLD_STATE->s_tmp.tmp_sys.sys_no) {
        case (1):
            createproc();       // LDST loads the state of this process right before the interrupt/trap
            break;
        case (2):
            killproc();         // Invokes schedule, no need to save state of this process
            break;
        case (3):
            semop();            // May invoke schedule, saves the process->p_s when added to blocked queue
            break;
        case (4):
            notused();
            break;
        case (5):
            trapstate();        // LDST loads the state of this process right before the interrupt/trap or kills process
            break;
        case (6):
            getcputime();       // LDST loads the state of this process right before the interrupt/trap
            break;
        case (7):
            waitforpclock();    // May invoke schedule, saves the process->p_s on LOCK operation
            break;
        case (8):
            waitforio();        // May invoke schedule, saves the process->p_s on LOCK operation
            break;
        default:
            trapsysdefault();   // LDST loads the sys trap handler from the new area state
            break;
    }

    // Reload the interrupted process on the CPU
    updateLastStartTime(process);
    LDST(SYS_TRAP_OLD_STATE);
}


/*
    Pass up Memory Managment trap or terminate the process
*/
void static trapmmhandler() 
{
    // Grab the interrupted process from the RQ
    proc_t* process = headQueue(readyQueue);

	// The process's old state area has been initialized and the appropiate new mm handler is present in the process's new mm area
    if (process->mm_trap_new_state != (state_t*)ENULL && process->mm_trap_old_state != (state_t*)ENULL) {
        // Update process start time as we load it unto the CPU
        updateLastStartTime(process);

        // Copy the interrupted process state
        *process->mm_trap_old_state = *MM_TRAP_OLD_STATE;

        // Load the Handler State routine specifics stored in this process's New State struct ptr (address set in SYS5) onto the CPU
        LDST(process->mm_trap_new_state);
    }
    else {
		// No handler address in the PTE for this trap or area to store its previous state
        killproc(process);
    }
}


/*
    Pass up Program trap or terminate the process
*/
void static trapproghandler()
{
    // Grab the interrupted process from the RQ
    proc_t* process = headQueue(readyQueue);

	// The process's old state area has been initialized and the appropiate new prog handler is present in the process's new prog area
    if (process->prog_trap_new_state != (state_t*)ENULL && process->prog_trap_old_state != (state_t*)ENULL) {
        // Update process start time as we load it unto the CPU
        updateLastStartTime(process);

        // Copy the interrupted process state (stored in 0x800) into the process's Prog Trap Old State Area
        *process->prog_trap_old_state = *PROG_TRAP_OLD_STATE;

        // Load the Handler State routine specifics stored in this process's New State struct ptr (address set in SYS5) onto the CPU
        LDST(process->prog_trap_new_state);
    } 
    else {
		// No handler address in the PTE for this trap or area to store its previous state
        killproc();
    }
}


/*
    When invoked, the kernel is loading this process on the CPU, so its previous start date must be updated
    to calculate the total time spent on the CPU since processes are pre-empted and/or removed from the RQ.
*/
void updateLastStartTime(proc_t* process) 
{
    // Grab the head process from the RQ
    long currentTime = 0;
    STCK(&currentTime);
    process->last_start_time = currentTime;
}


/*
    When the kernel removes the current prcoess from the RQ, so we use this to recalculate the
    total amount of time the removed process was on the CPU by adding the current time slice.
*/
void updateTotalTimeOnProcessor(proc_t* process) 
{
    // Grab the head process from the RQ
    long currentTime = 0;
    STCK(&currentTime);
    long prevTimeSlice = currentTime - process->last_start_time;
    process->total_processor_time += prevTimeSlice;
}


void trapinit()
{
    // Populate EVT with function addresses (Physical addresses from 0 to 0x800)
    *(int*)0x008 = (int)STLDMM;
    *(int*)0x00c = (int)STLDADDRESS;		   
    *(int*)0x010 = (int)STLDILLEGAL;		   
    *(int*)0x014 = (int)STLDZERO;			   
    *(int*)0x020 = (int)STLDPRIVILEGE;		   
    *(int*)0x08c = (int)STLDSYS;			   
    *(int*)0x94  = (int)STLDSYS9;			   
    *(int*)0x98  = (int)STLDSYS10;			   
    *(int*)0x9c  = (int)STLDSYS11;			   
    *(int*)0xa0  = (int)STLDSYS12;			   
    *(int*)0xa4  = (int)STLDSYS13;			   
    *(int*)0xa8  = (int)STLDSYS14;			   
    *(int*)0xac  = (int)STLDSYS15;			   
    *(int*)0xb0  = (int)STLDSYS16;			   
    *(int*)0xb4  = (int)STLDSYS17;			   
    *(int*)0x100 = (int)STLDTERM0;			   
    *(int*)0x104 = (int)STLDTERM1;			   
    *(int*)0x108 = (int)STLDTERM2;			   
    *(int*)0x10c = (int)STLDTERM3;			   
    *(int*)0x110 = (int)STLDTERM4;			   
    *(int*)0x114 = (int)STLDPRINT0;		    
    *(int*)0x11c = (int)STLDDISK0;
    *(int*)0x12c = (int)STLDFLOPPY0;
    *(int*)0x140 = (int)STLDCLOCK;

    // Allocate New and Old State Areas for Program Traps
    PROG_TRAP_OLD_STATE = (state_t*)BEGINTRAP;				  // Set pointer to address in Memory -> 76 bytes
    state_t* PROG_TRAP_NEW_STATE = PROG_TRAP_OLD_STATE + 1;   // Offset for New State area
    PROG_TRAP_NEW_STATE->s_sr.ps_m = 0;	 				      // Set memory management to physical addressing (no process virtualization)
    PROG_TRAP_NEW_STATE->s_sr.ps_s = 1;   				      // Switch to Supervisor Mode
    PROG_TRAP_NEW_STATE->s_sr.ps_int = 7; 				      // All interrupts disabled for process trap handler
    PROG_TRAP_NEW_STATE->s_sp = MEMSTART;					  // Set the global stack to the top, where the Kernel memory chunk is allocated
    PROG_TRAP_NEW_STATE->s_pc = (int)trapproghandler;	      // The address for this specific handler

    // Allocate New and Old State Areas for Memory Management Traps
    MM_TRAP_OLD_STATE = (state_t*)0x898;					  // Set pointer to address in Memory -> 76 bytes
    state_t* MM_TRAP_NEW_STATE = MM_TRAP_OLD_STATE + 1;       // Offset for New State area
    MM_TRAP_NEW_STATE->s_sr.ps_m = 0;	 				      // Set memory management to physical addressing (no process virutalization)
    MM_TRAP_NEW_STATE->s_sr.ps_s = 1;   					  // Switch to Supervisor Mode
    MM_TRAP_NEW_STATE->s_sr.ps_int = 7; 					  // All interrupts disabled for mm trap handler
    MM_TRAP_NEW_STATE->s_sp = MEMSTART;					      // Set the global stack to the top, where the Kernel memory chunk is allocated
    MM_TRAP_NEW_STATE->s_pc = (int)trapmmhandler;	          // The address for this specific handler

    // Allocate New and Old State Trap Areas for SYS Traps
    SYS_TRAP_OLD_STATE = (state_t*)0x930;					  // Set pointer to address in Memory -> 76 bytes
    state_t* SYS_TRAP_NEW_STATE = SYS_TRAP_OLD_STATE + 1;	  // Offset for New State area
    SYS_TRAP_NEW_STATE->s_sr.ps_m = 0;	 				      // Set memory management to physical addressing (no process virutalization)
    SYS_TRAP_NEW_STATE->s_sr.ps_s = 1;   				      // Switch to Supervisor Mode
    SYS_TRAP_NEW_STATE->s_sr.ps_int = 7; 				      // All interrupts disabled for mm trap handler
    SYS_TRAP_NEW_STATE->s_sp = MEMSTART;					  // Set the global stack to the top, where the Kernel memory chunk is allocated
    SYS_TRAP_NEW_STATE->s_pc = (int)trapsyshandler;		      // The address for this specific handler
}
