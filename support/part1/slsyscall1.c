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
#define	DO_TERMINATEPROC	SYS2	/* terminate process */
#define DO_SEMOP			SYS3
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

extern runnable_process_t terminal_processes[MAXTPROC];

// Cron table, semaphore, and related fields
typedef struct cron_entry_t {
    int sem;			
    long wakeUpTime;
} cron_entry_t;

extern cron_entry_t CRON_TABLE[MAXTPROC];
extern int cron_table_sem;
extern int wake_up_cron_sem;

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

    // Get the Terminal Process index from the CPU state
    int term_idx = terminal_sys_new_state.s_r[4];
    runnable_process_t* terminalProcess = &terminal_processes[term_idx];

    // Get the virtual address that will act as our buffer
    char* virtualAddr = (char*)terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[3];

    // Make the read request to the correct terminal device
    devreg_t* terminal = (devreg_t*)BEGINDEVREG + term_idx;		// Get terminal's device register from memory
    terminal->d_badd = terminalProcess->io_buffer;				// Buffer address stores the data we read from input
    terminal->d_op = IOREAD;									// Set the device operation status to READ (code 0)

    // Block the process by calling waitforio
    r4 = term_idx;
    DO_WAITIO();

    // Now the data has been stored in virtual address location and the get the results of the operation when its done
    int terminalStatus = terminal->d_stat;
    int actualLength = r2;
    int expectedLength = terminal->d_dadd;

    // Copy the data from the phyiscal buffer to the virtual address if the operation completed successfully
    if (terminalStatus == ENDOFINPUT) {
        // We read some amount of input but not the entire amount we defined in the iobuffer, 512
        if (actualLength > 0) {
            int i;
            for (i = 0; i < actualLength; i++) {
                virtualAddr[i] = terminalProcess->io_buffer[i];
            }
            terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = actualLength;
        }
        else {
            terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = -terminalStatus;
        }
    }
    else if (terminalStatus == NORMAL) {
        // In this case we read expected length bytes, so actual = expected
        int i;
        for (i = 0; i < expectedLength; i++) {
            virtualAddr[i] = terminalProcess->io_buffer[i];
        }
        terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = actualLength;
    }
    else {
        terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = -terminalStatus;
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
    // Get the Terminal Process state and number in the loaded New State Area 
    state_t terminal_sys_new_state;
    STST(&terminal_sys_new_state);

    // Get the Terminal Process index from the CPU state
    int term_idx = terminal_sys_new_state.s_r[4];
    runnable_process_t* terminalProcess = &terminal_processes[term_idx];

    // Get the virtual address that will hold the data
    char* virtualAddr = (char*)terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[3];
    int length = (int)terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[4];

    // Copy the data from the virtual address to the phyiscal io_buffer
    int i;
    for (i = 0; i < length; i++) {
        terminalProcess->io_buffer[i] = virtualAddr[i];
    }

    // Make the write request to the correct terminal device
    devreg_t* terminal = (devreg_t*)BEGINDEVREG + term_idx;		// Get terminal's device register from memory
    terminal->d_dadd = length;									// The size of the data buffer we are writing
    terminal->d_badd = terminalProcess->io_buffer;				// Buffer address stores the data we write to output
    terminal->d_op = IOWRITE;									// Set the device operation status to WRITE (code 1)

    // Block the process by calling waitforio
    r4 = term_idx;
    DO_WAITIO();

    // Get results of operation
    int terminalStatus = terminal->d_stat;
    int expectedLength = terminal->d_dadd;
    int actualLength = r2;

    if (terminalStatus == NORMAL) {
        if (expectedLength == 0) {
            terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = -ENDOFINPUT;
        }
        else {	// expected = actual
            terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = expectedLength;	// return number of bytes actually written
        }
    }
    else {
        terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = -terminalStatus;	// error flag
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
    // Get the Terminal Process index from the CPU state and update the Cron table
    state_t terminal_sys_new_state;
    STST(&terminal_sys_new_state);

    // Get the Terminal Process index from the CPU state
    int term_idx = terminal_sys_new_state.s_r[4];
    runnable_process_t* terminalProcess = &terminal_processes[term_idx];

    // Get the current time of day;
    long timeOfDay;
    STCK(&timeOfDay);

    // Get the delay from the CPU register D4
    int delay = terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[4];

    // Calculate the wakeup time
    long wakeUpTime = timeOfDay + delay;

    // Capture the Cron Table sempahore and update corresponding entry
    vpop lockTableOperation;
    lockTableOperation.op = LOCK;
    lockTableOperation.sem = &cron_table_sem;
    r3 = 1;
    r4 = (int)&lockTableOperation;
    DO_SEMOP();

    // Update the Cron Table entry with the the Wake Up Time (Non-accumulative)
    CRON_TABLE[term_idx].wakeUpTime = wakeUpTime;

    // Unlock the Cron Table semaphore, block the calling process, and unlock the Cron daemon
    vpop atomicSemOps[3];

    // Update the cron table semaphore and block the calling process
    atomicSemOps[1].op = LOCK;
    atomicSemOps[1].sem = &CRON_TABLE[term_idx].sem;

    // Unlock the Cron Table after updating the wake up time
    atomicSemOps[0].op = UNLOCK;
    atomicSemOps[0].sem = &cron_table_sem;

    // Add a resource (V) since Cron has one more process to manage
    atomicSemOps[2].op = UNLOCK;
    atomicSemOps[2].sem = &wake_up_cron_sem;

    // Make semops call
    r3 = 3;
    r4 = (int)&atomicSemOps;
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

    // Get the Terminal Process index from the CPU state 
    state_t terminal_sys_new_state;
    STST(&terminal_sys_new_state);

    // Return the time of day in the old sys state
    int term_idx = terminal_sys_new_state.s_r[4];
    runnable_process_t* terminalProcess = &terminal_processes[term_idx];
    terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_r[2] = timeOfDay;
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

    // No active processes, wake up cron which will call SYS2 and halt the nucleus
    if (active_t_processes == 0) {
        vpop wakeUpCronOperation;
        wakeUpCronOperation.op = UNLOCK;
        wakeUpCronOperation.sem = &wake_up_cron_sem;
        r3 = 1;
        r4 = (int)&wakeUpCronOperation;
        DO_SEMOP();
    }

    // Otherwise there are still active processes and we can free Pages allocated to this T-process
    state_t terminal_sys_new_state;
    STST(&terminal_sys_new_state);
    int term_idx = terminal_sys_new_state.s_r[4];

    // Free the pages from Segment 1 (User/Private data) 
    putframe(term_idx);

    // Kill Process
    DO_TERMINATEPROC();
}
