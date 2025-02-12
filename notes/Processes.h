/*
`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
	<---- Processes Review ---->
	
	A process is a running program that has its own address space (process virtualization / memory abstraction
	- Memory map
		- Text: compiled program
		- Data: initialized static data
		- Bss: unintialized statis data
		- Heap: dynamically allocated memory (~longlasting)
		- Stack: call stack for local vars and functions
	- System State: 
		- open files
		- pending signals
	- Processor State:
		- Program counter
		- CPU registers
		

	<---- Processes in a Multitasking Environment ---->

	- Multiple Concurrent Processes
		The system allows multiple processes to run simultaneously, improving efficiency and resource utilization.
		Each process is uniquely identified by a Process ID (PID), which helps the OS manage and track processes.

	- Handling Asynchronous Events
		Asynchronous events, such as hardware interrupts or signals, can occur at any time.
		The Operating System (OS) is responsible for handling these events promptly without disrupting ongoing processes.

	- Handling Long-Running Operations
		Processes may initiate operations (e.g., disk I/O, network requests) that require significant time to complete.
		While waiting, processes can enter a blocked or waiting state, allowing the CPU to switch to other ready-to-run processes.

	- Maximize CPU Utilization
		The OS aims to keep the CPU busy by ensuring there is always a process running.
		This involves efficient context switching, which allows the system to switch from one process to another seamlessly.

	- Process Suspension and Resumption
		Processes can be suspended (paused) to prioritize others or to handle resource constraints.
		When resuming a process, the OS must ensure that it continues exactly where it left off.

	- Context Saving and Restoration
		To suspend or switch processes, the OS saves the state of the current process (registers, program counter, memory state, etc.) in 
		a Process Control Block (PCB). When the process is resumed or switched back, the OS restores its state from the PCB to ensure continuity.


	<---- Processes Life Cycle ---->

	The Process Lifecycle consists of 4 major stages. Once the Operating System creates a process, the process is put in a waiting state.
	When the scheduler selects this process to run, it enters the running states. When the scheduler selects the next process to run,
	the current process is placed in the waiting state. This context switching will happen until the process is completed and it terminates.

	 created			   terminated
		|					   |
		V					   V
	 waiting <------------> running 
		^					   |
		|					   V
	    |------------------ blocked

	There is an additional state a process can enter: the blocked state. This occurs when a running process temporarily halts 
	to wait for an external event in the system, rather than waiting to be scheduled.

	The blocked state is invoked when a process invokes System calls such as I/O from a file.
	Reading a file often blocks a process because the operations to grab data from the disk are slower than CPU's execution
	of a process. ONLY in the waiting state, will the shecudler select a process to run. In the blocked state, the process waits to be put in the waiting state


	<---- Process List ---->

	How do we keep track of processes, the state they are in, and other important metadata?

	When the Operating System creates a new process, it adds a corresponding Process Table Entry (PTE) to a system-wide Process Table. 
	This PTE directly points to the Process Control Block (PCB) for that process which contains the following data:
		
	A Process List / Queue is a dynamic, linked list-like structure that contain PTEs (which have pointers to the PCBs for that type of process q).
	It is often used for traversal, scheduling, or managing processes in specific states (e.g., ready, waiting).
	In this design, the Process Table acts as a centralized record for all processes.


		Process Table                   Ready Process List								Waiting Process List
	+-----------------------+          +-----------+    +-----------+    +------+      +-----------+    +------+
	|   Open Process Table  |          | process1* | -> | process3* | -> | NULL |      | process2* | -> | NULL |
	+-----------------------+          +-----------+    +-----------+    +------+      +-----------+    +------+
	| proc_t process1       | 
	| proc_t process2       | 
	| proc_t process3       | 
	+-----------------------+

	NOTE:
	In our implementation combining the Process Table Entry (PTE) and Process Control Block (PCB) into a single structure, proc_t.
	This approach merges metadata typically stored in separate structures into one for simplicity or to align with specific design goals.

```````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````

	***HOCA IMPLEMENTATION SPECIFIC***

	Ready Queue:
		- Holds: Pointers to proc_t entries in the procTable that represent processes ready to run.
		- Links: Uses the p_link[0] field of proc_t for forming the linked list.

	Waiting Queue:
		- Holds: Pointers to proc_t entries in the procTable that represent processes waiting for an event or resource.
		- Links: Uses a specific p_link index (e.g., p_link[1]) for linking processes.

	Semaphore Queue:
		- Holds: Pointers to proc_t entries in the procTable that are blocked on semaphores.
		- Links: Uses another p_link index (e.g., p_link[2]) to organize processes waiting on semaphores.

	Process Table (procTable):
		- An array of proc_t structures representing all processes in the system (both active and free).
		- Acts as the universal source of process information.

	TYPE proc_t:
		- A combined Process Table Entry (PTE) and Process Control Block (PCB).
	  Key Fields:
		- p_link: An array of proc_link used to manage the process in multiple queues.
		- p_s: The processor state (e.g., CPU registers, program counter).
		- qcount: Tracks how many queues this process is in.
		- semvec: Tracks semaphores the process is waiting on.

	
	***HOCA IMPLEMENTATION SPECIFIC***
	The elements of each the process queue comes from the array procTable[MAXPROC]

	EACH `proc_t` IN THE `procTable` HAS A `p_link` ARRAY TO MANAGE ITS POSITION IN SPECIFIC QUEUES.

	Remember, `proc_t` is our 'Process Table Entry.'
	Let’s say a process is part of two queues, A and B. 
	When we access `p_link[queue_idx]` for each queue, we get the following structure:

	{
		int index;           // Index in the next process's p_link array for the same queue
		struct proc_t *next; // Pointer to the next proc_t in the specific queue
	}

	How it works:
	- NEXT:
	  - Points to the next process in the same queue.
	- INDEX:
	  - Indicates the index in the `p_link` array of the NEXT process to use to continue traversing in the same queue.****
	  - Since a process can belong to multiple queues, this index ensures correct traversal for the specific queue.****

	---
	Example:
	Let’s consider three processes (`proc1`, `proc2`, `proc3`) that are part of Queue A and Queue B

	-- proc1's `P_LINK` array: --

	p_link[2]: { index: 3, next: &proc2 }  // Points to proc2 in Queue A
	p_link[4]: { index: 1, next: &proc2 }  // Points to proc2 in Queue B

	-- proc2's `P_LINK` array: --

	p_link[3]: { index: 9, next: &proc3 }   // Points to proc3 in Queue A
	p_link[1]: { index: 2, next: &proc1 }   // End of Queue B

	-- proc3's `P_LINK` array: --

	p_link[9]: { index: 2, next: &proc1 } // End of Queue A

	************* Explaining the Indexes and Circular Queue Traversal *************

	1. In Queue A:
	   - We have `proc1->p_link[2].next = proc2`
	   - To continue traversing the queue, we use `proc1->p_link[2].index`, which is `3`.
	   - This indicates that in `proc2`, the queue continues at `proc2->p_link[3]`.

	2. At `proc2`:
	   - `proc2->p_link[3].next = proc3`
	   - Again, to continue traversing Queue A, we use the index `proc2->p_link[3].index`, which is `9`.
	   - This index will takes us to the next proc: `proc3->p_link[9]`.

	3. At `proc3`:
	   - This brings us here: `proc3->p_link[9].next = proc1` (Completeing Circular Queue A).
	   - The index to use for the next link is `proc3->p_link[9].index`, which is `2`.
	   - This brings us back to proc1, where `proc1->p_link[2]` indicates the next linke

	### Key Takeaways:
	- The `INDEX` field ensures the queue traversal is consistent, even when the queue spans different `p_link` arrays of multiple processes.
	- The `NEXT` field provides the actual pointer to the next process in the queue.
	- Together, the `NEXT` and `INDEX` fields allow for circular traversal of the same queue, ensuring no confusion with other queues the process might belong to.

	This design allows a single `proc_t` to exist in multiple queues simultaneously while maintaining efficient queue traversal
	by allowing us to access the same proc_t in every queue it is present in and enable centralized process management.

`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
	NOTES on HOCA IMPL:
		The index field in a proc_link tells you which `p_link[]` slot to 
		use in the next `proc_t` for the queue linkage,
		ensuring consistent traversal across processes even if they use
		different slots for the same queue.

		Yes, that’s exactly what’s happening ->
		each process may use a different p_link[] slot for the same queue.
		In other words :
		The Tail Pointer’s index might be, say, 3, which the current tail process uses for that queue.
		The New Process might find a free index, say 5, and use p_link[5] to link into the same queue.

`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
*/
