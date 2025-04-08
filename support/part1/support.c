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

/*
	Execution Flow of the HOCA Memory Managment System:
		- A user-mode T-process generates a Virtual Address with a segment number, page number, and offset.
		- The segment number identifies a segment in virtual memory, part of the virtual address.
		- The CRP register holds the base address of the process’s segment table.
		- The MMU adds the Segment number (scaled by descriptor size) to the CRP to locate the corresponding segment descriptor (seg entry = CPR + seg #)
		- The segment descriptor provides the base address of the page table for that segment.
		- The MMU indexes into the page table using the virtual page number (segEntry.PTA + pageNumber)
		- In the correspoding Page Table and Page Table Entry, the MMU acquires the physical Page Frame number,
		- which is combined with the offset to compute the final physical address.
*/

/*
	Each Terminal Process (T-Process) effectively has 2 Segment Tables:
      -> User Mode Segment Table (used when executing user code):
		 This Segment Table maps:
		   - Segment 1: Private pages (code, data, stack) for that specific T-process.
		   - Segment 2: Shared pages (for inter-process communication/synchronization).

	  -> Supervisor Mode Segment Table (used during trap handling):
		 This Segment table maps to Segment 0 (in addition to Segment 1 and 2) which maps the following
		   - Page 2: Device registers
		   - Nucleus pages, marked not present for protection
		   - Pages for SUPPORT-LEVEL CODE/DATA (TEXT, DATA, BSS)
		   - Pages for HANDLER STACKS (e.g., Tsysstack[i], Tmmstack[i])
		    (
			  There are several T-processes running at one time, and no assurance that when 
			  one process enters the support level, none of the others will also enter it concurrently.

			  Also, a T-process executing in the support level could cause another support level
			  trap to occur. The only way this can happen is if a process executing in the support
			  level SYS handler causes a memory management trap (consider the case where a Tprocess
			  tries to copy data from from a virtual address that is not in physical memory). 

			  Therefore the stack used by the memory management trap handler in the support level MUST BE DISJOINT
			  from that of the SYS (and program) trap handlers.
		   )
		   - Support-level pages, marked present

		   This table is used when the T-process traps into supervisor mode (e.g., SYS, MM traps). 
		   Using SYS5, the process specifies new trap handlers and corresponding states that use this privileged segment table.
*/

// Stored in support level data (PA)
#define KERNEL_PAGES 256

// Kernel Routines
#define DO_SEMOP			SYS1
#define DO_CREATEPROC		SYS3
#define DO_SPECTRAPVEC		SYS5
#define	DO_WAITCLOCK		SYS7	/* delay on the clock semaphore */

// Global CPU registers
register int r2 asm("%d2");
register int r3 asm("%d3");
register int r4 asm("%d4");

// Bootstrap loader object code
int bootcode[] = {
	0x41f90008,
	0x00002608,
	0x4e454a82,
	0x6c000008,
	0x4ef90008,
	0x0000d1c2,
	0x787fb882,
	0x6d000008,
	0x10bc000a,
	0x52486000,
	0xffde4e71
};

// Pointers for Terminal process SYS/MM Disjointed Stacks
extern int Tsysstack[5];
extern int Tmmstack[5];
extern int Scronstack;

// Declare function addresses for Support segments
extern int startt1();
extern int etext();
extern int startd1();
extern int edata();
extern int startb1();
extern int end();

// Header declarations of local routines
void static p1a();
void static cron();
void static tprocess();

#define START_SUPPORT_TEXT ((int)startt1 / PAGESIZE)
#define END_SUPPORT_TEXT ((int)etext / PAGESIZE)
#define START_SUPPORT_DATA ((int)startd1 / PAGESIZE)
#define END_SUPPORT_DATA ((int)edata / PAGESIZE)
#define START_SUPPORT_BSS ((int)startb1 / PAGESIZE)
#define END_SUPPORT_BSS ((int)end / PAGESIZE)
#define START_DEVICE_REG ((int)BEGINDEVREG / PAGESIZE)
#define END_DEVICE_REG ((int)((devreg_t*)BEGINDEVREG + 5) / PAGESIZE)

// Not used
pd_t shared_pd_table[32];

typedef struct runnable_process_t {
	sd_t user_mode_sd_table[32];		// Uses Segment Entry 1, 2. Segment 1 manages the user private pages (code, data, stack) for that process & Segmenet 2 manages shared pages
	sd_t kernel_mode_sd_table[32];		// Uses Segment Entry 0, 1, 2. Segment 0 maps pages for Support data, text, bss, & handler (restricted, requires privilege mode)

	pd_t user_mode_pd_table[32];					// The Page Table for Segment Entry 1 (user/private pages)
	pd_t kernel_mode_pd_table[KERNEL_PAGES];		// The Page Table for Segment Entry 0 (kernel pages)

	// Stores the process old state and the appropiate trap handler state
	state_t SUPPORT_SYS_TRAP_OLD_STATE;
	state_t SUPPORT_SYS_TRAP_NEW_STATE;

	state_t SUPPORT_PROG_TRAP_OLD_STATE;
	state_t SUPPORT_PROG_TRAP_NEW_STATE;

	state_t SUPPORT_MM_TRAP_OLD_STATE;
	state_t SUPPORT_MM_TRAP_NEW_STATE;

	// IO devices can only handle Physical Addresses
	// SYS10 (wrt to terminal) -> copy data from VA to PA/iobuffer -> call devreg on iobuffer -> waitforio by passing addressing to this field
	char io_buffer[512];

} runnable_process_t;

// Terminal Process Table
const runnable_process_t terminal_processes[MAXTPROC];


// Cron Daemon process, struct, semaphores, and fields
runnable_process_t system_cron_process;

typedef struct cron_entry_t {
	int sem;			
	long wakeUpTime;
} cron_entry_t;


// Cron Table
cron_entry_t CRON_TABLE[MAXTPROC];

// Cron table semaphore
int cron_table_sem = 1;

// Semaphore for exclusive access to Cron routine
int wake_up_cron_sem = 0;

// Global counter for active T-processes
int active_t_processes = MAXTPROC;


/*
	This function initializes all segment and page tables. In particular it
	protects the nucleus from the support level by marking the nucleus
	pages as not present. It initializes the page module by calling
	pageinit(). It calls getfreeframe() and loads the bootcode on the page
	frame. It passes control to p1a() which runs with memory mapping
	on and interrupts enabled.
*/
void p1()
{
	// Pageinit() initialize Stack pointers: Tsysstack[i], Tmmstack[i], Scronstack, Spagedstack, Sdiskstack
	// It also ensures that we can allocate frames via getfreeframe() by marking USUABLE physical pages

	// RECALL: Multiple T-processes can enter the support level concurrently via SYS traps, 
	// or MM traps that are handled using the memory space of page frames mapped by Segment 0 (Nucleus data)
	// If these processes share the same stack space, concurrent modifications would occur overwriting crucial data
	// Hence each T-process will get a SYS/Prog Trap stack -> Tsysstack[i] and a MM Trap stack -> Tmmstack[i]
	pageinit();


	// ** Initialize Each Terminal Process with a User and Privileged Mode Segment Table **
	int i, j, k;
	for (i = 0; i < MAXTPROC; i++) {
		runnable_process_t* terminalProcess = &terminal_processes[i];

		// User Mode Segment Table only has 2 Segments for the user's private pages (Segment 1) and globally shared pages (Segment 2)
		terminalProcess->user_mode_sd_table[1].sd_pta = terminalProcess->user_mode_pd_table;
		terminalProcess->user_mode_sd_table[1].sd_pta = terminalProcess->user_mode_pd_table;
		terminalProcess->user_mode_sd_table[2].sd_pta = shared_pd_table;
		
		// Init Segment 1 & 2 of the User Mode Segment Table with Presence bit, Access Protection bits, and Page Table length
		terminalProcess->user_mode_sd_table[1].sd_p = 1;
		terminalProcess->user_mode_sd_table[1].sd_prot = 7;
		terminalProcess->user_mode_sd_table[1].sd_len = 32;

		terminalProcess->user_mode_sd_table[2].sd_p = 1;
		terminalProcess->user_mode_sd_table[2].sd_prot = 7;
		terminalProcess->user_mode_sd_table[2].sd_len = 32;

		// Init all other Segments in the User Mode Table to have presence bit off
		terminalProcess->user_mode_sd_table[0].sd_p = 0;
		for (j = 3; j < 32; j++) {
			terminalProcess->user_mode_sd_table[j].sd_p = 0;
		}

		// Privileged/Kernel Mode Segment Table has 3 segments for user/private pages (Segment 1), globally shared pages (Segment 2), and for privileged data (support/kernel) pages (Segment 0
		terminalProcess->kernel_mode_sd_table[0].sd_pta = terminalProcess->kernel_mode_pd_table;
		terminalProcess->kernel_mode_sd_table[1].sd_pta = terminalProcess->user_mode_pd_table;
		terminalProcess->kernel_mode_sd_table[2].sd_pta = shared_pd_table;

		// Init Segment 0, 1, 2 of the Kernel Mode Segment Table with Presence bit, Access Protection bits, and Page Table length
		terminalProcess->kernel_mode_sd_table[0].sd_p = 1;
		terminalProcess->kernel_mode_sd_table[0].sd_prot = 7;
		terminalProcess->kernel_mode_sd_table[0].sd_len = KERNEL_PAGES;

		terminalProcess->kernel_mode_sd_table[1].sd_p = 1;
		terminalProcess->kernel_mode_sd_table[1].sd_prot = 7;
		terminalProcess->kernel_mode_sd_table[1].sd_len = 32;

		terminalProcess->kernel_mode_sd_table[2].sd_p = 1;
		terminalProcess->kernel_mode_sd_table[2].sd_prot = 7;
		terminalProcess->kernel_mode_sd_table[2].sd_len = 32;

		// Init all other Segments in the Kernel Mode Table to have presence bit off
		for (k = 3; k < 32; k++) {
			terminalProcess->kernel_mode_sd_table[k].sd_p = 0;
		}

		// Initialize the process's Kernel-mode Page Table
		//
		// This table maps virtual pages to physical page frames.
		// When the CPU accesses a virtual address, the MMU uses this table to find the corresponding Page Descriptor
		// which is then used to get the Page Frame # that is used to calculate a Physical Address in memory (see pic).
		//
		// In kernel mode, we allow access to specific memory segments:
		// 1. Device registers
		// 2. Support TEXT segment (from START_SUPPORT_TEXT to END_SUPPORT_TEXT)
		// 3. Support DATA segment (from START_SUPPORT_DATA to END_SUPPORT_DATA) 
		// 4. Support BSS segment (from START_SUPPORT_BSS to END_SUPPORT_BSS)
		// 5. SEG0 (specifically page 2)
		//
		// For each page descriptor i (0-256), we determine if it falls within the address range
		// of one of these segments. If it does, we mark that page as present (pd_p = 1).
		// Otherwise, we mark it as not present (pd_p = 0) to prevent access.
		//
		// The segment boundaries (START_*/END_*) are calculated by dividing the physical
		// addresses of Segment Markers in physical memory (like startt1, etext) by PAGESIZE to get page frame #'s.

		int pgFrame;
		for (pgFrame = 0; pgFrame < KERNEL_PAGES; pgFrame++) {
			// the Kernel Mode Page Table (maps Segment 0) from the Kernel SD Table
			pd_t* kernelPageTable = terminalProcess->kernel_mode_sd_table[0].sd_pta;

			// Each page descriptor maps exactly One-to-One with each Page Frame in Phyiscal Memory since those pages are allocated in a predefined order
			kernelPageTable[pgFrame].pd_frame = pgFrame;

			// Check if this page frame corresponds to SEG0 (Page 2), if so set presence bit ON
			if (pgFrame == 2) {
				kernelPageTable[pgFrame].pd_p = 1;	// Mark page frame as present
			}
			// Check if this page frame corresponds to the DEVICE REGISTERS AREA
			else if (pgFrame >= START_DEVICE_REG && pgFrame <= END_DEVICE_REG) {
				kernelPageTable[pgFrame].pd_p = 1;
			}
			// Check if this page frame corresponds to SUPPORT TEXT
			else if (pgFrame >= START_SUPPORT_TEXT && pgFrame <= END_SUPPORT_TEXT) {
				kernelPageTable[pgFrame].pd_p = 1;
			}
			// Check if this page frame corresponds to SUPPORT DATA
			else if (pgFrame >= START_SUPPORT_DATA && pgFrame <= END_SUPPORT_DATA) {
				kernelPageTable[pgFrame].pd_p = 1;
			}
			// Check if this page frame corresponds to SUPPORT BSS
			else if (pgFrame >= START_SUPPORT_BSS && pgFrame <= END_SUPPORT_BSS) {
				kernelPageTable[pgFrame].pd_p = 1;
			}
			// Check if this page frame corresponds to the T-SYSSTACK
			else if (pgFrame == ((int)Tsysstack[i] / PAGESIZE)) {
				kernelPageTable[pgFrame].pd_p = 1;
			}
			// Check if this page frame corresponds to the T-MMSTACK
			else if (pgFrame == ((int)Tmmstack[i] / PAGESIZE)) {
				kernelPageTable[pgFrame].pd_p = 1;
			}
			// Otherwise this page frame corresponds to another memory segment we cannot provide access to initially, so Page Frame as not present
			else { 
				kernelPageTable[pgFrame].pd_p = 0;
			}
		}
	}


	// ** Initialize Single System Process (Cron daemon) with only a Privileged Mode Segment Table **

	// The code and data for system processes will reside in Segment 0 with the nucleus
	// Set the Page Table address for Segment 0 to the Kernel Mode Page table
	system_cron_process.kernel_mode_sd_table[0].sd_pta = &system_cron_process.kernel_mode_pd_table;

	// Init Segment 0 of the Privileged Mode Segment Table with Presence bit, Access Protection bits, and Page Table length
	system_cron_process.kernel_mode_sd_table[0].sd_p = 1;
	system_cron_process.kernel_mode_sd_table[0].sd_prot = 7;
	system_cron_process.kernel_mode_sd_table[0].sd_len = KERNEL_PAGES;

	// Set the presence bit off for all other segments in the Privileged Mode Segment Table
	for (i = 1; i < 32; i++) {
		system_cron_process.kernel_mode_sd_table[i].sd_p = 0;
	}

	// Initialize the Kernel Page Descriptor Table for Segment 0 of the Cron's Privileged Mode Segment Table
	int pgFrame;
	for (pgFrame = 0; pgFrame < KERNEL_PAGES; pgFrame++) {
		pd_t* kernelPageTable = system_cron_process.kernel_mode_pd_table;

		// Each Page Descriptor maps exactly One-to-One with each Page Frame in Phyiscal Memory since those pages are allocated in a predefined order
		kernelPageTable[pgFrame].pd_frame = pgFrame;

		// Check if this page frame corresponds to SEG0 (Page 2), if so set presence bit ON
		if (pgFrame == 2) {
			kernelPageTable[pgFrame].pd_p = 1;	// Mark page frame as present
		}
		// Check if this page frame corresponds to SUPPORT TEXT
		else if (pgFrame >= START_SUPPORT_TEXT && pgFrame <= END_SUPPORT_TEXT) {
			kernelPageTable[pgFrame].pd_p = 1;
		}
		// Check if this page frame corresponds to SUPPORT DATA
		else if (pgFrame >= START_SUPPORT_DATA && pgFrame <= END_SUPPORT_DATA) {
			kernelPageTable[pgFrame].pd_p = 1;
		}
		// Check if this page frame corresponds to SUPPORT BSS
		else if (pgFrame >= START_SUPPORT_BSS && pgFrame <= END_SUPPORT_BSS) {
			kernelPageTable[pgFrame].pd_p = 1;
		}
		// Check if this page frame corresponds to the SCRONSTACK
		else if (pgFrame >= (Scronstack / PAGESIZE) && pgFrame <= (((int)end / PAGESIZE) + 14)) {
			kernelPageTable[pgFrame].pd_p = 1;
		}
		// Otherwise this page frame corresponds to another memory segment we cannot provide access to initially, so page frame as not present
		else {
			kernelPageTable[pgFrame].pd_p = 0;
		}
	}


	// Allocate page 31 in each T-process's user-mode page table (Segment 1) to load the bootcode.
	// The bootcode initializes the user program by setting up trap vectors (SYS5s) and then transfers control to the actual user code.
	for (i = 0; i < MAXTPROC; i++) {
		runnable_process_t* terminalProcess = &terminal_processes[i];

		// Get the User Mode Page Table (maps Segment 1) of this Terminal Process
		pd_t* userPageTable = terminalProcess->user_mode_sd_table[1].sd_pta;

		// Turn off the presence bit for page frames (initially we dont not want to allocate Pages unneccesarily)
		int pgDesc;
		for (pgDesc = 0; pgDesc < 31; pgDesc++) {
			userPageTable[pgDesc].pd_p = 0;
		}

		// Allocate a free page frame for this process
		userPageTable[31].pd_frame = getfreeframe(i, 31, 1);

		// Load the boot code into this Page Frame by using physical address
		int* pageStart = (int*)(userPageTable[31].pd_frame * PAGESIZE);
		int j;
		for (j = 0; j < 10; j++) {
			*(pageStart + j) = bootcode[j];
		}
	}

	// Initialize Cron Table semaphores and wake up times
	int k;
	for (k = 0; k < 20; k++) {
		CRON_TABLE[k].sem = 0;			// Process semaphore init to 0
		CRON_TABLE[k].wakeUpTime = -1;	// Set Process wakeup time to -1 to signal no delay requested
	}

	// Create p1a process state
	state_t p1aState;
	p1aState.s_sr.ps_s = 1;			// Supervisor/Privilege mode on
	p1aState.s_sr.ps_m = 1;			// Memory management on
	p1aState.s_sr.ps_int = 0;		// Interrupts on
	p1aState.s_pc = (int)p1a;		// Set program counter to p1a routine
	p1aState.s_sp = Scronstack;		// Set the stack pointer to the Cron deamon stack address in the Stack's segment

	// Set CPU Root Pointer to the (only) Kernel Mode Segment Table of the Cron daemon
	p1aState.s_crp = &system_cron_process.kernel_mode_sd_table;		

	// Load p1a process
	LDST(&p1a);
}


/*
	This routine will execute the process will create T-processes in a loop that.
	Each T-process will be in supervisor mode as it starts up because it has to do its SYS5’s, etc. In setting
	up the states for the created T - processes, the root process should give each a disjoint stack. 
*/
void static p1a() 
{
	// Create a 'generic' process state that is privileged that will enable the set up of the Trap Areas for each T-process via SYS5
	state_t privilegedProcessState;

	int i;
	for (i = 0; i < MAXTPROC; i++) {
		// Prepare the initial process state for each T-process using the 'generic' process state declared above
		runnable_process_t* terminalProcess = &terminal_processes[i];

		// Set CPU Root Pointer to the process's Kernel Mode Segment Table
		privilegedProcessState.s_crp = terminalProcess->kernel_mode_sd_table;

		// Set the Stack pointer to the correct SYS Trap Stack
		privilegedProcessState.s_sp = Tsysstack[i];

		// Set program counter to tprocess() to specify the process's Trap Areas
		privilegedProcessState.s_pc = (int)tprocess;

		// Pass the terminal process identifier in D4 of the child process state
		privilegedProcessState.s_r[4] = i;

		// Turn on Supervisor mode, Interrupts and Memory Management
		privilegedProcessState.s_sr.ps_s = 1;		
		privilegedProcessState.s_sr.ps_m = 1;
		privilegedProcessState.s_sr.ps_int = 0;

		// Reference the initial process state address in System Old Trap Area register 'D4'
		r4 = (int)&privilegedProcessState;

		// Create terminal process and add it to Run Queue
		DO_CREATEPROC();
	}

	// Prepare to Cron process state
	state_t cronProcessState;

	// Store the current state which has the proper memory managment and CRP set
	STST(&cronProcessState);

	// Set the program counter to the cron() function and reset stack pointer
	cronProcessState.s_pc = (int)cron;
	cronProcessState.s_sp = Scronstack;		

	// Load the Cron 'Daemon' Process
	LDST(&cronProcessState);
}


/*
	This function does the appropriate SYS5s and loads a state with user
	mode and PC = 0x80000 + 31 * PAGESIZE.
*/
void static tprocess()
{
	// Recall that SYS5 specifies a trap vector by telling the nucleus:
	// - Where to save the process’s state when a specific trap occurs (D3 -> old state pointer)
	// - What processor state to load when handling that trap (D4 -> new state pointer)
	// Each T-process needs to initialize the memory locations of these areas for each Trap Type
	state_t priviligedTerminalProcessState;

	// Store the privileged T-Process state (added to the RQ via CREATEPROC) which invoked this routine
	STST(&priviligedTerminalProcessState);

	// Get the corresponding Terminal Process for the Trap State Vector setup
	int term_idx = priviligedTerminalProcessState.s_r[4];
	runnable_process_t* terminalProcess = &terminal_processes[term_idx];


	/*** Specify System Trap Areas ***/
	r2 = SYSTRAP;

	// Set the old state address in D3
	r3 = (int)&terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE;

	// Set up the new state pointed at by D4 with Privilege, Interrupts and Memory Mangement modes ON
	terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE.s_r[4] = term_idx;
	terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE.s_sr.ps_s = 1;
	terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE.s_sr.ps_m = 1;
	terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE.s_sr.ps_int = 0;
	terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE.s_pc = (int)slsyshandler;
	terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE.s_sp = Tsysstack[term_idx];
	terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE.s_crp = terminalProcess->kernel_mode_sd_table;
	r4 = (int)&terminalProcess->SUPPORT_SYS_TRAP_NEW_STATE;

	// invoke SYS5
	DO_SPECTRAPVEC();


	/*** Specify Program Trap Areas ***/
	r2 = PROGTRAP;

	// Set the old state address in D3
	r3 = (int)&terminalProcess->SUPPORT_PROG_TRAP_OLD_STATE;

	// Set up the new state pointed at by D4 with Privilege, Interrupts and Memory Mangement modes ON
	terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE.s_r[4] = term_idx;
	terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE.s_sr.ps_s = 1;
	terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE.s_sr.ps_m = 1;
	terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE.s_sr.ps_int = 0;
	terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE.s_pc = (int)slproghandler;
	terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE.s_sp = Tsysstack[term_idx];
	terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE.s_crp = terminalProcess->kernel_mode_sd_table;
	r4 = (int)&terminalProcess->SUPPORT_PROG_TRAP_NEW_STATE;

	// invoke SYS5
	DO_SPECTRAPVEC();


	/*** Specify Memory Management Trap Areas ***/
	r2 = MMTRAP;

	// Set the old state address in D3
	r3 = (int)&terminalProcess->SUPPORT_MM_TRAP_OLD_STATE;

	// Set up the new state pointed at by D4 with Privilege, Interrupts, and Memory Mangement modes ON
	terminalProcess->SUPPORT_MM_TRAP_NEW_STATE.s_r[4] = term_idx;
	terminalProcess->SUPPORT_MM_TRAP_NEW_STATE.s_sr.ps_s = 1;
	terminalProcess->SUPPORT_MM_TRAP_NEW_STATE.s_sr.ps_m = 1;
	terminalProcess->SUPPORT_MM_TRAP_NEW_STATE.s_sr.ps_int = 0;
	terminalProcess->SUPPORT_MM_TRAP_NEW_STATE.s_pc = (int)slmmhandler;

	terminalProcess->SUPPORT_MM_TRAP_NEW_STATE.s_crp = terminalProcess->kernel_mode_sd_table;
	r4 = (int)&terminalProcess->SUPPORT_MM_TRAP_NEW_STATE;

	// invoke SYS5
	DO_SPECTRAPVEC();


	/*** Prepare the 'real' Terminal Process State ***/
	state_t terminalProcessState;
	terminalProcessState.s_sr.ps_s = 0;										// Supervisor/Privileged Mode off
	terminalProcessState.s_sr.ps_m = 1;										// Memory Management on
	terminalProcessState.s_sr.ps_int = 0;									// Interrupts on
	terminalProcessState.s_crp = terminalProcess->user_mode_sd_table;		// Set the CPU Root Pointer to the correct T process's segment table
	
	// Set the terminal process's private memory location in virtual memory. 
	// The user stack is at the top of Segment 1 (in virtual memory), right above the bootcode!
	terminalProcessState.s_pc = (int)(0x80000 + PAGESIZE * 31);				// Bootcode location in Virtual memory
	terminalProcessState.s_sp = (int)(0x80000 + PAGESIZE * 32) - 2;			// User stack location in Virtual memory

	// Run the 'real' Terminal Process and increment our active T-Process counter
	active_t_processes++;
	LDST(&terminalProcessState);
}


/*
	Support Level Trap Handlers
*/

/*
	This function checks the validity of a page fault. It calls getfreeframe() to allocate a free page frame. If necessary it calls
	pagein() and then and it updates the page tables. It uses the semaphore sem_mm to control access to the critical section that updates
	the shared data structures. The pages in segment two are a special case in this function (N/A).
*/
void static slmmhandler()
{
	// A Terminal Process requests more pages for Segment 1 (User/Private data)
	// However since all Pages presence bits are off (0) initially (except Page Frame 31)(except Page Frame 31)(except Page Frame 31)(except Page Frame 31)(except Page Frame 31)(except Page Frame 31)(except Page Frame 31)(except Page Frame 31)(except Page Frame 31), a Memory Managment Trap is thrown

	// Get the Terminal Process State when the Trap was thrown
	state_t terminal_mm_new_state;
	STST(&terminal_mm_new_state);

	// Get the Terminal Process index from the CPU state
	int term_idx = terminal_mm_new_state.s_r[4];
	runnable_process_t* terminalProcess = &terminal_processes[term_idx];

	// Get the Segment number/entry and Page Number
	tmp_t temp_mm = terminal_mm_new_state.s_tmp;
	unsigned trapType = temp_mm.tmp_mm.mm_typ;
	int pageNumber = temp_mm.tmp_mm.mm_pg;
	int segmentNumber = temp_mm.tmp_mm.mm_seg;

	// Handle MM traps
	if (trapType == PAGEMISS) {
		// Only handle MM traps for Segment 1 and 2 for the User Segment Table
		if (segmentNumber != 1 && segmentNumber != 2) { 
			DO_TTERMINATE();
		}

		// Get the Segment Descriptor entry 
		sd_t segmentDesc = terminalProcess->user_mode_sd_table[segmentNumber];

		// Check whether Page # is invalid if sd_len is smaller since the Page Entry is calculated with sd.PTA + Page #
		if (pageNumber >= segmentDesc.sd_len) {
			DO_TTERMINATE();
		}

		// Get the exact Page Descriptor entry if Page number is valid
		pd_t* pageDesc = &terminalProcess->user_mode_sd_table[segmentNumber].sd_pta[pageNumber];

		// Allocate a free Page Frame for this Page Descriptor and mark it as present
		pageDesc->pd_frame = getfreeframe();
		pageDesc->pd_p = 1; 

		// Zero/initialize data isn't necessary as it will get overwritten when used
	}
	else {
		// Access Protection Violation, sd.R or sd.W or sd.E is 0 OR The Segment is missing OR Page number is invalid
		DO_TTERMINATE();
	}
}


/*
	This function has a switch statement and it calls the functions in slsyscall1.c and slsyscall2.c
*/
void static slsyshandler()
{
	// Execution Flow:
	//  For User System Calls (SYS9-SYS17), virtual addresses are used which won't work with nucleus. 
	//  These will pass up these up to Support which will be able to handle the virtual addresses
	//
	//  - A T-process executes a SYS instruction using a virtual address, causing a Trap (e.g., for I/O or delay).
	//	- The trap is handled by the nucleus by trapsyshandler(), which saves the T-process's state in the SYS TRAP OLD AREA and then calls trapsysdefault()
	//	- If the process has previously executed a SYS5 to install a trap vector :
	//    -> The nucleus loads the "new state" from the SYS5 in trapsysdefault() - which will have the PC to syshandler() and the properties we defined in TPROCESS()
	//	  -> Control is then transferred to this function (slsyshandler) in privileged mode.
	//	- This function then looks at the syscall number and dispatches to the appropriate handler.

	// Get the Terminal Process state when the Trap was thrown
	state_t terminal_sys_new_state;
	STST(&terminal_sys_new_state);

	// Get the Terminal Process index from the CPU state
	int term_idx = terminal_sys_new_state.s_r[4];
	runnable_process_t* terminalProcess = &terminal_processes[term_idx];

	// Recall that the Support SYS Trap Old Area address points to the process's 'old_sys_trap_state' after SYS5
	// Then in trapsysdefault, we copy the SYS_TRAP_OLD_AREA into process->sys_trap_old_state which is equivalent to Support SYS Trap Old Area
    switch (terminalProcess->SUPPORT_SYS_TRAP_OLD_STATE.s_tmp.tmp_sys.sys_no) {
        case (9):
			readfromterminal();
            break;
        case (10):
			writefromterminal();
            break;
        case (13):
			delay();
            break;
        case (16):
			gettimeofday();
            break;
        case (17):
			terminate();
            break;
        default:
			HALT(); // TODO ASK
            break;
    }
}


/*
	This functions calls terminate() when a Program Trap occurs.
*/
void static slproghandler()
{
	DO_TTERMINATE();
}


/*
	This function releases processes which delayed themselves, and it shuts down if there are no
	T-processes running. cron should be in an infinite loop and should block on the pseudoclock if there 
	is no work to be done. If possible you should synchronize delay and cron, otherwise one point will be lost
*/
void static cron()
{
	// Block the Cron process unconditionally once (In the future, the PC is maintained and the while loop below will prevent the execution of first SEMOP again)
	vpop lockCronOperation;
	lockCronOperation.op = LOCK;
	lockCronOperation.sem = &wake_up_cron_sem;
	r3 = 1;
	r4 = (int)&lockCronOperation;
	DO_SEMOP();

	// Critical section where Cron job releases sleeping process
	while (1) {

		// Terminate Cron if there are no t-processes running
		if (active_t_processes == 0) {
			DO_TTERMINATE();
		}

		// Allow only 1 process to read/write the Cron table
		vpop lockTableOperation;
		lockTableOperation.op = LOCK;
		lockTableOperation.sem = &cron_table_sem;
		r4 = (int)&lockTableOperation;
		DO_SEMOP();

		// Check if there are any T-processes being delayed
		int delayedProcesses = FALSE;

		int i;
		for (i = 0; i < MAXTPROC; i++) {
			// Check if this process can be released
			long currentTime = DO_GETTOD();

			// At least one process has been delayed
			if (CRON_TABLE[i].wakeUpTime != -1) {
				delayedProcesses = TRUE;
			}

			// Wake up this process if we have passed the wakeup time deadline
			if (CRON_TABLE[i].wakeUpTime >= currentTime) {
				vpop wakeUpOperation;
				wakeUpOperation.op = UNLOCK;
				wakeUpOperation.sem = &CRON_TABLE[i].sem;
				r3 = 1;
				r4 = (int)&wakeUpOperation;
				DO_SEMOP();
			}
		}

		// Release the Cron table semaphore to allow access to the table before we potentially block ourselves
		vpop unlockTableOperation;
		unlockTableOperation.op = UNLOCK;
		unlockTableOperation.sem = &cron_table_sem;
		r3 = 1;
		r4 = (int)&unlockTableOperation;
		DO_SEMOP();

		// There are no delayed processes we could possibly release, block ourselves
		if (!delayedProcesses) {
			DO_WAITCLOCK();
		}
	}
}

