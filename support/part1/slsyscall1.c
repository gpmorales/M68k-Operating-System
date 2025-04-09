/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include <string.h>
#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/vpop.h"
#include "../../h/procq.e"
#include "../../h/asl.e"
#include "./h/tconst.h"


// Kernel Routines
#define DO_SEMOP			SYS1
#define	DO_WAITIO			SYS8	/* delay on a io semaphore */

// Global CPU registers
register int r2 asm("%d2");
register int r3 asm("%d3");
register int r4 asm("%d4");

#define KERNEL_PAGES 256

/*
	Virtual Addresses, I/O Buffers, and the MMU in Support Level Routines:
	- Routines here run with privilege mode AND Memory Managment on which means
	  All addresses used in C code are virtual when the MMU is on.
	- Ex: BEGINDEVREG = 0x1400 is used as a virtual address, but...
	- In the Kernel Mode Segment Table, Segment 0’s Page Table is set up to maps virtual pages 1:1 to physical pages.
	- So Virtual Address 0x1400 = Physical Address 0x1400 via segment 0’s mapping.

	Since I/O is done with device registers that live in restricted physical memory
	having a priv mode and memory managment on is not suffice as we still need a way to reach this space with the correct Physical Address

	Even though io_buffer lives in physical memory (support-level .data), when the MMU is on and you're running in privileged mode:
	You access io_buffer using a virtual address

	The segment 0 mapping in the Kernel Page Table makes sure:
		- virtual address of io_buffer == physical address of io_buffer
*/

typedef struct runnable_process_t {
	sd_t user_mode_sd_table[32];		
	sd_t kernel_mode_sd_table[32];	

	pd_t user_mode_pd_table[32];					
	pd_t kernel_mode_pd_table[KERNEL_PAGES];	

	state_t SUPPORT_SYS_TRAP_OLD_STATE;
	state_t SUPPORT_SYS_TRAP_NEW_STATE;
	state_t SUPPORT_PROG_TRAP_OLD_STATE;
	state_t SUPPORT_PROG_TRAP_NEW_STATE;
	state_t SUPPORT_MM_TRAP_OLD_STATE;
	state_t SUPPORT_MM_TRAP_NEW_STATE;
	char io_buffer[512];

} runnable_process_t;

extern const runnable_process_t terminal_processes[MAXTPROC];

// Cron table, semaphore, and related fields
typedef struct cron_entry_t {
	int sem;			
	long wakeUpTime;
} cron_entry_t;

extern cron_entry_t CRON_TABLE[MAXTPROC];
extern int cron_table_sem;

// Global counter for active T-processes
extern int active_t_processes;


/*
	Requests that the invoking T-process be suspended until a line of input has been
	read from the associated terminal. The data read should be placed in the virtual
	memory of the T-process executing the SYS9, at the (virtual) address given in D3 at
	the time of the call. The count of the number of characters actually read from the
	terminal should be available in D2 when the T-process is continued. An attempt to
	read when there is no more data available should return the negative of the ‘‘End of
	Input’’ status code in D2. If the operation ends with a status other than ‘‘Successful
	Completion’’ or ‘‘End of Input’’ (as described above), the negative of the Status
	register value should be returned in D2. Any negative numbers returned in D2 are ‘‘error flags.’’
*/
void readfromterminal()
{
	// Get the Terminal Process state and number in the loaded New State Area 
	state_t terminal_sys_new_state;
	STST(&terminal_sys_new_state);

	// Get the virtual address that will act as our buffer
	char* virtualAddr = (char*)r3;

	// Get the Terminal Process index from the CPU state
	int term_idx = terminal_sys_new_state.s_r[4];
	runnable_process_t* terminalProcess = &terminal_processes[term_idx];

	// Make the read request to the correct terminal device
    devreg_t* terminal = (devreg_t*)BEGINDEVREG + term_idx;		// Get terminal's device register from memory
    terminal->d_stat = DEVNOTREADY;								// Set status code to non 0 value as we prepare to request I/O
    terminal->d_dadd = 128;										// The size of the buffer (max 128 bytes)
    terminal->d_badd = terminalProcess->io_buffer;				// Buffer address stores the data we read from input
    terminal->d_op = IOREAD;									// Set the device operation status to READ (code 0)

	// Block the process by calling waitforio
	r4 = (int)&term_idx;
	DO_WAITIO();

	// Now the data has been stored in virtual address location and the operation is done
	int terminalStatus = terminal->d_stat;
	int length = terminal->d_dadd;

	if (terminalStatus == NORMAL) {
		// Copy the data from the phyiscal buffer to the virtual address if hte operation completed successfully
		for (int i = 0; i < length; i++) {
			virtualAddr[i] = terminalProcess->io_buffer[i];
		}
		r2 = length;
	}
	else {
		r2 = -terminalStatus;
	}
}


/*
	Requests that the T-process be suspended until a line (string of characters) of output
	has been written on the terminal associated with the process. The virtual address of
	the first character of the line to be written will be in D3 at the time of the call. 
	The count of the number of characters to be written will be in D4. The count of the
	number of characters ACTUALLY written should be placed in D2 upon completion of
	the SYS10. As in SYS9, a non-successful completion status will cause an error flag
	to be returned instead of the character count.
*/
void writetoterminal()
{
	// TODO ASK
	// Get the Terminal Process state and number in the loaded New State Area 
	state_t terminal_sys_new_state;
	STST(&terminal_sys_new_state);

	// Get the virtual address that will hold the data
	char* virtualAddr = (char*)r3;
	int length = (int)r4;

	// Get the Terminal Process index from the CPU state
	int term_idx = terminal_sys_new_state.s_r[4];
	runnable_process_t* terminalProcess = &terminal_processes[term_idx];

	// Copy the data from the virtual address to the phyiscal io_buffer
	for (int i = 0; i < length; i++) {
		terminalProcess->io_buffer[i] = virtualAddr[i];
	}

	// Make the write request to the correct terminal device
    devreg_t* terminal = (devreg_t*)BEGINDEVREG + term_idx;		// Get terminal's device register from memory
    terminal->d_stat = DEVNOTREADY;								// Set status code to non 0 value as we prepare to request I/O
    terminal->d_dadd = length;									// The size of the data buffer we are writing
    terminal->d_badd = terminalProcess->io_buffer;				// Buffer address stores the data we write to output
    terminal->d_op = IOWRITE;									// Set the device operation status to WRITE (code 1)

	// Block the process by calling waitforio
	r4 = (int)&term_idx;
	DO_WAITIO();

	int terminalStatus = terminal->d_stat;
	if (terminalStatus == NORMAL || terminalStatus == ENDOFINPUT) {
		r2 = terminal->d_dadd;			// return number of bytes actually written
	}
	else {
		r2 = -terminalStatus;			// error flag
	}
}


/*
	On entry, D4 contains the number of microseconds for which the invoker is to be
	delayed. The caller is to be delayed at least this long, and not substantially longer.
	Since the nucleus controls low-level scheduling decisions, all that you can ensure is
	that the invoker is not dispatchable until the time has elapsed, but becomes dispatchable shortly thereafter.
*/
void delay()
{
	// Get the delay from the CPU register D4
	int delay = r4;
	
	// Get the current time of day;
	long timeOfDay;
	STCK(&timeOfDay);

	// Calculate the wakeup time
	long wakeUpTime = timeOfDay + delay;

	// Get the Terminal Process index from the CPU state and update the Cron table
	state_t terminal_sys_new_state;
	STST(&terminal_sys_new_state);

	// Capture the Cron Table sempahore to write this value 
	vpop lockTableOperation;
	lockTableOperation.op = LOCK;
	lockTableOperation.sem = &cron_table_sem;
	r4 = (int)&lockTableOperation;
	DO_SEMOP();

	// Update the Cron Table entry with the the Wake Up Time (Non-accumulative)
	int term_idx = terminal_sys_new_state.s_r[4];
	CRON_TABLE[term_idx].wakeUpTime = wakeUpTime;

	// Unlock the Cron Table semaphore
	vpop unlockTableOperation;
	unlockTableOperation.op = UNLOCK;
	unlockTableOperation.sem = &cron_table_sem;
	r4 = (int)&unlockTableOperation;
	DO_SEMOP();

	// Remove the calling process off the Run Queue via SEMOP after release Cron Table semaphore
	vpop lockProcessOperation;
	lockProcessOperation.op = LOCK;
	lockProcessOperation.sem = &CRON_TABLE[term_idx].sem;
	r4 = (int)&lockProcessOperation;
	DO_SEMOP();
}


/*
	Returns the value of the time-of-day clock in D2.
*/
void gettimeofday()
{
	// Get time of day
	long timeOfDay;
	STCK(&timeOfDay);

	// Return value in D2
	r2 = timeOfDay;
}


/*
	Terminates the T-process. When all T-processes have terminated, your operating
	system should shut down. Thus, somehow the ‘system’ processes created in the
	support level must be terminated after all five T-processes have terminated. Since
	there should then be no dispatchable or blocked processes, the nucleus will halt.
*/
void terminate()
{
	// Decrease the number of active T-processes
	active_t_processes--;

	// No active processes, no reason for nucleus or support routines to continue execution
	if (active_t_processes == 0) {
		HALT();
	}

	// TODO ASK
	// Otherwise there are still active processes and we can free the Segments and Pages allocated to this T-process
	state_t terminal_sys_new_state;
	STST(&terminal_sys_new_state);

	int term_idx = terminal_sys_new_state.s_r[4];
	runnable_process_t* terminalProcess = &terminal_processes[term_idx];

	// Free the pages from Segment 1 (User/Private data) 
	pd_t* userPageTable = terminalProcess->user_mode_sd_table[1].sd_pta;

	int i;
	for (i = 0; i < 32; i++) {
		userPageTable[i].pd_p = 0;				// Set presence bit off
		userPageTable[i].pd_frame = 0;			// Set the page frame # to 0 to indicate it is not in use
	}

	//DO_KILLPROC();
}
