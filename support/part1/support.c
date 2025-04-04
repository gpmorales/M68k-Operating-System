/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/procq.e"
#include "../../h/asl.e"

/*
	This module coordinates the initalization of the support level, it services traps that are passed up, and it creates any necessary system processes.
	It has the following functions : p1(), p1a(), slsyshandler(), slmmhandler(), slproghandler(), tprocess() and cron().
*/

/*
	Execution Flow of the HOCA Memory Managment System:
		- A user-mode T-process generates a Virtual Address with a segment number, page number, and offset.
		- The segment number identifies a segment in virtual memory, part of the virtual address.
		- The CRP register holds the base address of the process’s segment table.
		- The MMU adds the Segment number (scaled by descriptor size) to the CRP to locate the corresponding segment descriptor.
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
#define TERMINAL_PROCESS 2
#define KERNEL_PAGES 256

// boot strap loader object code
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

void static p1a();

#define START_SUPPORT_TEXT ((int)startt1 / PAGESIZE)
#define END_SUPPORT_TEXT ((int)etext / PAGESIZE)
#define START_SUPPORT_DATA ((int)startd1 / PAGESIZE)
#define END_SUPPORT_DATA ((int)edata / PAGESIZE)
#define START_SUPPORT_BSS ((int)startb1 / PAGESIZE)
#define END_SUPPORT_BSS ((int)end / PAGESIZE)
#define START_DEVICE_REG ((int)BEGINDEVREG / PAGESIZE)
#define END_DEVICE_REG ((int)((devreg_t*)BEGINDEVREG + 5) / PAGESIZE)

// Not used
const pd_t shared_pd_table[32];

typedef struct runnable_process_t {
	sd_t user_mode_sd_table[32];		// Uses Segment Entry 1, 2. Segment 1 manages the user private pages (code, data, stack) for that process & Segmenet 2 manages shared pages
	sd_t kernel_mode_sd_table[32];		// Uses Segment Entry 0, 1, 2. Segment 0 maps pages for Support data, text, bss, & handler (restricted, requires privilege mode)

	pd_t user_mode_pd_table[32];		// The Page Table for Segment Entry 1 (user/private pages)
	pd_t kernel_mode_pd_table[256];		// The Page Table for Segment Entry 0 (kernel pages)

	// For User System Calls (SYS9-SYS17), virtual addresses are used which won't work wtih nucleus. 
	// These will pass up these up to Support which will be able to handle the virtual addresses

	// T Process -> SYS9 -> EVT (Privilege Trap) -> Save T Process state in SYS TRAP OLD AREA -> systraphandler -> trapsysdefault -> load the state from proc.SYS_NEW_STATE
	// The state that we load would be NEW_SUPPORT_SYS_TRAP_AREA (SYS5 stores SYS TRAP NEW AREA into NEW_SUPPORT_SYS_TRAP_AREA)
	// and this would call the PC would be at slsystraphandler 

	// Init states to point to the correspoinding function handlers, set MM on, and set the Stack (pg. 14)
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
const runnable_process_t terminal_processes[2];

// Cron Daemon
runnable_process_t system_cron_process;


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

	// ** Initialize Each Terminal Process with a User and Privileged Mode Segment Table **
	int i, j, k;
	for (i = 0; i < TERMINAL_PROCESS; i++) {
		runnable_process_t terminalProcess = terminal_processes[i];

		// User Mode Segment Table only has 2 Segments for the user's private pages (Segment 1) and globally shared pages (Segment 2)
		terminalProcess.user_mode_sd_table[1].sd_pta = &terminalProcess.user_mode_pd_table;
		terminalProcess.user_mode_sd_table[2].sd_pta = &shared_pd_table;
		
		// Init Segment 1 & 2 of the User Mode Segment Table with Presence bit, Access Protection bits, and Page Table length
		terminalProcess.user_mode_sd_table[1].sd_p = 1;
		terminalProcess.user_mode_sd_table[1].sd_prot = 7;
		terminalProcess.user_mode_sd_table[1].sd_len = 32;

		terminalProcess.user_mode_sd_table[2].sd_p = 1;
		terminalProcess.user_mode_sd_table[2].sd_prot = 7;
		terminalProcess.user_mode_sd_table[2].sd_len = 32;

		// Init all other Segments in the User Mode Table to have presence bit off
		terminalProcess.user_mode_sd_table[0].sd_p = 0;
		for (j = 3; j < 32; j++) {
			terminalProcess.user_mode_sd_table[j].sd_p = 0;
		}

		// Privileged/Kernel Mode Segment Table has 3 segments for user/private pages (Segment 1), globally shared pages (Segment 2), and for privileged data (support/kernel) pages (Segment 0
		terminalProcess.kernel_mode_sd_table[0].sd_pta = &terminalProcess.kernel_mode_pd_table;
		terminalProcess.kernel_mode_sd_table[1].sd_pta = &terminalProcess.user_mode_pd_table;
		terminalProcess.kernel_mode_sd_table[2].sd_pta = &shared_pd_table;

		// Init Segment 0, 1, 2 of the Kernel Mode Segment Table with Presence bit, Access Protection bits, and Page Table length
		terminalProcess.kernel_mode_sd_table[0].sd_p = 1;
		terminalProcess.kernel_mode_sd_table[0].sd_prot = 7;
		terminalProcess.kernel_mode_sd_table[0].sd_len = 32;

		terminalProcess.kernel_mode_sd_table[1].sd_p = 1;
		terminalProcess.kernel_mode_sd_table[1].sd_prot = 7;
		terminalProcess.kernel_mode_sd_table[1].sd_len = 32;

		terminalProcess.kernel_mode_sd_table[2].sd_p = 1;
		terminalProcess.kernel_mode_sd_table[2].sd_prot = 7;
		terminalProcess.kernel_mode_sd_table[2].sd_len = 32;

		// Init all other Segments in the Kernel Mode Table to have presence bit off
		for (k = 3; k < 32; k++) {
			terminalProcess.kernel_mode_sd_table[k].sd_p = 0;
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
			pd_t page = terminalProcess.kernel_mode_pd_table[pgFrame];

			// Each page descriptor maps exactly One-to-One with each Page Frame in Phyiscal Memory since those pages are allocated in a predefined order
			page.pd_frame = pgFrame;

			// Check if this page frame corresponds to SEG0 (Page 2)
			if (pgFrame == 2) {
				page.pd_p = 1;	// Mark page frame as present
			}
			// Check if this page frame corresponds to the DEVICE REGISTERS AREA
			else if (pgFrame >= START_DEVICE_REG && pgFrame <= END_DEVICE_REG) {
				page.pd_p = 1;  // Mark page frame as present
			}
			// Check if this page frame corresponds to SUPPORT TEXT
			else if (pgFrame >= START_SUPPORT_TEXT && pgFrame <= END_SUPPORT_TEXT) {
				page.pd_p = 1;
			}
			// Check if this page frame corresponds to SUPPORT DATA
			else if (pgFrame >= START_SUPPORT_DATA && pgFrame <= END_SUPPORT_DATA) {
				page.pd_p = 1;
			}
			// Check if this page frame corresponds to SUPPORT BSS
			else if (pgFrame >= START_SUPPORT_BSS && pgFrame <= END_SUPPORT_BSS) {
				page.pd_p = 1;
			}
			// Otherwise this page frame corresponds to another memory segment we cannot provide access to initially, so page frame as not present
			else { 
				page.pd_p = 0;	
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
	system_cron_process.kernel_mode_sd_table[0].sd_len = 32;

	// Set the presence bit off for all other segments in the Privileged Mode Segment Table
	for (i = 1; i < 32; i++) {
		system_cron_process.kernel_mode_sd_table[i].sd_p = 0;
	}

	// Initialize the Page Descriptor Table for Segment 0 of the Cron's Privileged Mode Segment Table
	int pgFrame;
	for (pgFrame = 0; pgFrame < KERNEL_PAGES; pgFrame++) {
		pd_t page = system_cron_process.kernel_mode_pd_table[pgFrame];

		// Each page descriptor maps exactly One-to-One with each Page Frame in Phyiscal Memory since those pages are allocated in a predefined order
		page.pd_frame = pgFrame;

		// Check if this page frame corresponds to SEG0 (Page 2)
		if (pgFrame == 2) {
			page.pd_p = 1;
		}
		// Check if this page frame corresponds to SUPPORT TEXT
		else if (pgFrame >= START_SUPPORT_TEXT && pgFrame <= END_SUPPORT_TEXT) {
			page.pd_p = 1;
		}
		// Check if this page frame corresponds to SUPPORT DATA
		else if (pgFrame >= START_SUPPORT_DATA && pgFrame <= END_SUPPORT_DATA) {
			page.pd_p = 1;
		}
		// Check if this page frame corresponds to SUPPORT BSS
		else if (pgFrame >= START_SUPPORT_BSS && pgFrame <= END_SUPPORT_BSS) {
			page.pd_p = 1;
		}
		// Otherwise this page frame corresponds to another memory segment we cannot provide access to initially, so page frame as not present
		else {
			page.pd_p = 0;
		}
	}


	// Pageinit() initialize Stack pointers: Tsysstack[i], Tmmstack[i], Scronstack, Spagedstack, Sdiskstack
	// It also ensures that we can allocate frames via getfreeframe() by marking USUABLE physical pages

	// RECALL: Multiple T-processes can enter the support level concurrently via SYS traps, 
	// or MM traps that are handled using the memory space of page frames mapped by Segment 0 (Nucleus data)
	// If these processes share the same stack space, concurrent modifications would occur overwriting crucial data
	// Hence each T-process will get a SYS Trap stack -> Tsysstack[i] and a MM Trap stack -> Tmmstack[i]
	pageinit();

	// Allocate page 31 in each T-process's user-mode page table (Segment 1) to load the bootcode.
	// The bootcode initializes the user program by setting up trap vectors (SYS5s) 
	// and then transfers control to the actual user code.
	for (i = 0; i < TERMINAL_PROCESS; i++) {
		runnable_process_t terminalProcess = terminal_processes[i];

		// Get the Page table for Segment 1 of the User Mode Segement Table
		pd_t* userPageTable = terminalProcess.user_mode_sd_table[1].sd_pta;

		// Turn off the presence bit for page frames (initially we dont not want to allocate dpages unneccesarily)
		int pgDesc;
		for (pgDesc = 0; pgDesc < 31; pgDesc++) {
			userPageTable[pgDesc].pd_p = 0;
		}

		// Allocate a free page frame for this process
		userPageTable[31].pd_frame = getfreeframe(i, 31, 1);

		// Load the boot code into this page frame
		int* pageStart = (int*) (userPageTable[31].pd_frame * PAGESIZE);
		int j;
		for (j = 0; j < 12; i++) {
			*(pageStart + j) = bootcode[j];
		}
	}

	// Load the p1a/Cron process
	state_t p1aState;
	p1aState.s_sr.ps_m = 1;			// Memory management on
	p1aState.s_sr.ps_int = 0;		// Interrupts on
	p1aState.s_sr.ps_s = 1;			// Supervisor/Privilege mode on
	p1aState.s_pc = (int)p1a;		// Set program counter to p1a routine
	p1aState.s_sp = Scronstack;		// Set the stack pointer to the Cron deamon stack address in the Stack's segment
    // CPU Root Pointer will point to the Kernel Mode Segment Descriptor table of the Cron daemon so the MMU knows where to read data from physical memory
	p1aState.s_crp = &system_cron_process.kernel_mode_sd_table;

	LDST(&p1a);
}



/*
	This routine will execute the process will create T-processes in a loop that.
	Each T-process will be in supervisor mode as it starts up because it has to do its SYS5’s, etc. In setting
	up the states for the created T - processes, the root process should give each a disjoint stack. 

	This can be done by setting the CRP register and also the SP register in the state of the created process:
	<create a generic start state for T - processes>;
	for (each terminal) {
		< set the CRP to the Segment Table for this terminal in start state>;
		< set SP to Tsysstack[terminal] in start state>;
		<create process>;
	}
*/
void static p1a() 
{

}


