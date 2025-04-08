/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/vpop.h"
#include "../../h/procq.e"
#include "../../h/asl.e"
#include "./h/tconst.h"


// Kernel Routines
#define DO_SEMOP			SYS1

// Global CPU registers
register int r2 asm("%d2");
register int r3 asm("%d3");
register int r4 asm("%d4");

#define KERNEL_PAGES 256

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
	//

}


/*
	Requests that the T-process be suspended until a line (string of characters) of output
	has been written on the terminal associated with the process. The virtual address of
	the first character of the line to be written will be in D3 at the time of the call. 
	The count of the number of characters to be written will be in D4. The count of the
	number of characters actually written should be placed in D2 upon completion of
	the SYS10. As in SYS9, a non-successful completion status will cause an error flag
	to be returned instead of the character count.
*/
void writetoterminal()
{

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

	// Capture the Cron Table sempahore to write this value 
	vpop lockTableOperation;
	lockTableOperation.op = LOCK;
	lockTableOperation.sem = &cron_table_sem;
	r4 = (int)&lockTableOperation;
	DO_SEMOP();


	// Get the Terminal Process index from the CPU state and update the Cron table
	state_t terminal_sys_new_state;
	STST(&terminal_sys_new_state);

	int term_idx = terminal_sys_new_state.s_r[4];
	CRON_TABLE[term_idx].wakeUpTime = wakeUpTime;

	// Block the calling process on the delay
	vpop lockProcessOperation;
	lockProcessOperation.op = LOCK;
	lockProcessOperation.sem = &CRON_TABLE[term_idx].sem;
	r4 = (int)&lockProcessOperation;
	DO_SEMOP();


	// Unlock the Cron table semaphore
	vpop unlockTableOperation;
	unlockTableOperation.op = UNLOCK;
	unlockTableOperation.sem = &cron_table_sem;
	r4 = (int)&unlockTableOperation;
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

	// No active processes, the Cron daemon will never need to run again
	if (active_t_processes == 0) {
		HALT();
	}

	// Otherwise there are still active processes and we can free the Segments and Pages allocated to this T-process
	state_t terminal_sys_new_state;
	STST(&terminal_sys_new_state);
	int term_idx = terminal_sys_new_state.s_r[4];
	runnable_process_t* terminalProcess = &terminal_processes[term_idx];

	// TODO ASK
	sd_t* segmentOneDesc = &terminalProcess->user_mode_pd_table[1];
	pd_t* userPageTable = segmentOneDesc->sd_pta;

	// Segment is not allocated anymore
	segmentOneDesc->sd_p = 0;
	segmentOneDesc->sd_pta = NULL;

	// Free the pages in the User Space Page table
	int i;
	for (i = 0; i < 32; i++) {
		userPageTable[i].pd_p = 0;
		userPageTable[i].pd_frame = -1;
		//userPageTable[i].
	}

	//putframe();
	//DO_KILLPROC();
}
