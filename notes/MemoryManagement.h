/*
`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
	<---- Memory Management System in the Operating System ---->

		### Segemented Memory ###
		* Segments are swapped between disk and main memory as needed and vary in size
		* Program segments correspond to blocks of program code such as procedures or functions
		* Data segements correspond to data structures such as stacks, queues, or graphs
		* The Operating System knows start and size of each segement in physical memory
		* Each segment is atomic, either the whole segment is in RAM, or none of the segment is in RAM (no internal fragementation)
		* A segment in memory can only be replaced by a segment of the same SIZE or smaller
		* Segmentation can result in external memory fragmentation; a lot of small segments with gaps in between
		* Large segements may not be allowed into memory more than often
		* Segments can be pushed togther to limit fragmentation and allow for large segements to be loaded

		### Paged Memory ###
		* Memory is split up into small, equal sized sections called Page Frames (think of fixed-size PHYSICAL MEMORY CONTAINERS)
		* A single application may have multiple virtual segments (contiguous) that each occupy multiple pages 
		* From a process's perspective it has all of memory and starts at the largest memory location available
		* A Page Table is used to map where different pages of the program in logical / virtual memory are located in physical memory ****
		* In a Page Table Entry, a page frame number (PFN) points to the start of the Page Frame in physical memory.
		  The offset within the page is handled separately when translating virtual to physical addresses.
		  So if a program's memory starts at offset 100 in a 4KB page frame, the page table entry just points to the start of that
		  page frame, and the address translation mechanism uses the offset from the virtual address to locate the exact memory location.


	%%%%%%%%% KEY INSIGHTS %%%%%%%%%

	 PAGED MEMORY IS FUNDAMENTALLY A COMBINATION OF SEGMENTATION AND PAGING:
	- From the process's perspective, it sees contiguous memory segments (logical/virtual view)
	- Internally, the OS breaks these segments into Pages (virtual) and maps them to Pages Frame (each segment is scattered across physical memory page frames)!
	- Each segment can have a different length and number of valid Page Table entries that store its mappings to Physical Pages from the virtual Pages
	- This allows flexible memory allocation while maintaining the illusion of a continuous memory space for the process

	IN OTHER WORDS EACH PROGRAM GETS IT OWN MEMORY, WHICH MEANS:
	THE SEGMENT DESCRIPTOR TABLE AND ALL THE PAGE TABLES IT POINTS TO ARE SPECIFIC TO ONE PROCESS (PROGRAM) AT A TIME.

	- A segment in virtual memory is composed of multiple 512-byte pages.
	- The segment defines a contiguous virtual memory range (e.g., 4KB).
	- That range is internally divided into 512-byte chunks — these are the "pages" (8 Pages for 4KB segment)
	- Each virtual page is translated individually using that specifc segment’s Page Table!!!!!!

	** Each virtual page is mapped to exactly one 512-byte physical page frame.
	** It’s a 1-to-1 mapping — not split or scattered.
	** That entire 512-byte virtual page is placed into one physical page frame.

	%%%%%%%%% KEY INSIGHTS %%%%%%%%%



	<---- EMACSIM Memory Management System ---->

	Note that an EMACSIM address can be interpreted either as a PHYSICAL ADDRESS or as a VIRTUAL ADDRESS.
	An address is interpreted as a physical address whenever the memory mapping mechanism is off (i.e. SR.M=0)
	and interpreted as a virtual address otherwise (i.e.SR.M=1).

	Example:
		proc_t* process = ...
		process.s_r.ps_m = 0;
	
	In EMACSIM, the virtual space is 16M with addresses ranging from 0 to 0xffffff. 
	The virtual address space is divided into 512-byte units called PAGES. 
	The physical address space is 128K and it is divided into 512-byte units called PAGE FRAMES.

	When memory mapping is in effect (SR_M=1), a virtual address is automatically mapped into a physical address.
	The CPU Root Pointer, CRP, contains the physical address of a Segment Table, which is composed of THIRTY TWO entries.

	Note that each proc_t (process) has a segment table address register called *s_crp
	This points to a Segment Descriptor struct:

		typedef struct sd_t {
			unsigned un	 :18,		// unused
				 sd_p	 :1,		// presence bit P
				 sd_prot :3,		// access protection bits (READ, WRITE, EXECUTE)
					 sd_len	 :10;	// page table length
			pd_t	 *sd_pta;		// physical address of the corresponding page table
		} sd_t;

	 - Each Segement Table Entry is eight bytes long and is referred to as a Segment Descriptor (SD) in types.h

	  ---------					  ----------
	 |   CRP   | --------------> |   SD 0   |
	 |		   |                 |----------|
	  ---------	                 |   SD 1   |
	 		                     |----------|
								 |   SD 2   |
	 		                     |----------|
								 |   ...    |
	 		                     |----------|
								 |  SD 31   |
	 		                      ----------

	Segement Descriptor:
	Len -> Maximum valid page number within segment (one less than actual segment size)
	E,W,R -> Execute, Write and Read access rights for the segment, respectively; if 1, the corresponding access right is permitted
	P -> Presence bit; if 0, a missing segment trap is generated
	PTA -> Physical Address of the corresponding page table

	
	## Pages and Page Tables ##
	The PAGE TABLE has 1024 ENTRIES, with each entry being a 4-byte page descriptor (PD). 
	The Len field in each Segment Descriptor (SD) specifies the maximum valid Page number for that specific Segment.
	This means each Segment can use a different number of Page Table Entries, up to the maximum of 1024. The Len field allows the system to define the exact number of valid pages for a particular segment, providing flexibility in memory allocation while using a single, large page table structure.


	*** RELATIONSHIP BETWEEN SEGMENTs, SEGMENT DESCRIPTOR, PAGE TABLE ENTRY/ADDRESS, and PHYSICAL MEMORY ***

	1) Segmentation exists at the logical (virtual) level — the program is divided into
	   several segments (e.g., code, stack, heap), and each segment has a Segment Descriptor (SD).

		The Segment Descriptor includes:

		- sd_pta: pointer to the Page Table
		- sd_len: number of valid pages in that segment

		NOTE: Each Segment is divided into Pages (each 512 bytes) in virtual memory.

		**** The Page Table for a Segment MAPS EACH of THESE VIRTUAL PAGE in that SEGMENT to PHYSICAL PAGE FRAMEs in Physical Memory ****

		NOTE: Each Segment Descriptor will have a Page Table address that points to a Table with up to 1024 entries...

		typedef struct pd_t {
			unsigned un	  :14,		// unused
				 pd_p	  :1,		// presence bit
				 pd_m	  :1,		// modified bit
				 pd_r	  :1,		// reference bit
					 pd_frame :15;		// page frame number
		} pd_t;

		...Each entry being a Page Descriptor of type:
		
		P -> Presence bit; if 0, a missing page trap is generated
		M -> Modified bit; set to 1 by the processor whenever the page is modified
		R -> Reference bit; set to 1 whenever the page is read, written or executed
		PageFrame -> if the Presence bit is set, contains the number of the page frame to which this page is mapped; otherwise undefined


*/
