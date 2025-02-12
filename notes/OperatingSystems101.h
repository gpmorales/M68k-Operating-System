/*
`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
	<---- The Operating System ---->

	An operating system (OS) is THE software layer that acts as an intermediary between the computer's hardware and 
	application programs (software), providing essential services and resource management for the efficient and secure
	execution of tasks. Specifically it provides controlled access to the following resources:
		– CPU (central processing unit)
		– Memory
		– Display, keyboard, mouse
		– Persistent storage
		– Network

	*** The OS Core Responsibilities (Kernel) ***

		- Process Management:
			Manages the lifecycle of processes, including creation, scheduling, execution, and termination.
			Ensures fair CPU time allocation using algorithms like round-robin, priority-based, or multi-level queues.
			Handles inter-process communication (IPC) through mechanisms like shared memory, pipes, or message queues.

		- Memory Management:
			Allocates and deallocates memory to processes dynamically.
			Implements virtual memory, enabling processes to use more memory than physically available.
			Prevents memory leaks and ensures isolation between processes to maintain security.

		- File System Management:
			Provides a structured way to store, organize, and retrieve data on storage devices.
			Supports file operations (create, read, write, delete) and ensures data integrity through permissions and journaling.

		- Device Management:
			Manages input/output devices (e.g., keyboard, disk, network) via device drivers and I/O scheduling.
			Abstracts hardware complexities, exposing a consistent interface to application programs.

		- Resource Allocation:
			Coordinates resource sharing among processes, including CPU, memory, storage, and I/O devices, while avoiding 
			deadlocks and resource starvation.


	<---- The Kernel ---->

	The kernel, often referred to as the NUCLEUS of the Operating System, is the core component responsible for managing hardware resources 
	and providing essential services to system processes. IT IS A COLLECTION OF ROUTINES that are executed as needed, such as during
	interrupts, system calls, or context switches. The kernel's code is executed by the CPU during these specific events, layering in between the execution of user processes. 

	*** Operating System vs Kernel ***
	+--------------------+--------------------------------------------------------------+-----------------------------------------------------------------------------+
	| Aspect	         | Kernel	                                                    | Operating System (OS)                                                       |
	+--------------------+--------------------------------------------------------------+-----------------------------------------------------------------------------+
	| Scope              | The kernel is the core part of the OS, directly managing     | The OS is broader, encompassing the kernel along with system                |
	|                    | hardware resources, and acting as a bridge between hardware  | utilities, libraries, user interfaces (e.g., CLI/GUI), and applications.    |
	|                    | and software.                                                |                                                                             |
	+--------------------+--------------------------------------------------------------+-----------------------------------------------------------------------------+
	| Role	             | Acts as a resource manager, handling critical tasks such     | Provides a complete environment for users to run applications               |
	|                    | as CPU scheduling, memory allocation, and hardware control.  | by integrating kernel functions with utilities and user-friendly interfaces.|
	+--------------------+--------------------------------------------------------------+-----------------------------------------------------------------------------+
	| Execution Mode     | Operates in **privileged kernel mode**, allowing direct      | Includes components that primarily run in **user mode**, with               |
	|                    | access to hardware and complete control over resources.      | some interacting indirectly with the kernel for resource access.            |
	+--------------------+--------------------------------------------------------------+-----------------------------------------------------------------------------+
	| Components		 | Includes low-level operations such as process management,    | Combines the kernel with system libraries (e.g., libc), utilities           |
	|                    | memory management, file systems, and device drivers.         | (e.g., file browsers, task schedulers), and user interfaces (CLI/GUI).      |
	+--------------------+--------------------------------------------------------------+-----------------------------------------------------------------------------+
	| Direct Interaction | Directly interacts with hardware devices using device        | Provides an abstraction layer for users and applications to                 |
	|                    | drivers, ensuring efficient hardware communication.          | interact with hardware via kernel-managed services.                         |
	+--------------------+--------------------------------------------------------------+-----------------------------------------------------------------------------+

	NOTE:
	The kernel interacts with the CPU directly. It is the only part of the operating system with the authority (privileged access) 
	to control the CPU and other hardware components. It can also do the following due to its elevated privileges:
	- Access restricted regions of memory
	– Modify the memory management unit
	– Set timers
	– Define interrupt vectors
	– Halt the processor


	<---- Multi-Tasking & interactions of the CPU with the Kernel ---->

	If the CPU is executing a process, the kernel's code or OS management routines cannot run simultaneously. 
	This raises a critical issue: what happens if a process fails to trigger an interrupt within a reasonable time frame?
	In such cases, the kernel's routines for managing system resources remain unexecuted or delayed, potentially causing severe system instability.

	**** Solution -> Pre-emptive Multi-tasking ***

	To address this, we leverage preemptive multitasking. By programming a timer interrupt, we ensure the CPU periodically 
	pauses its current task to allow the kernel to execute crucial operations, such as process scheduling and resource management. 
	This mechanism ensures other processes can run by forcing a context swith

	When a timer interrupt is triggered, the CPU stops executing the current user process and immediately switches to a kernel routine. 
	This is how the kernel regains control of the CPU and prepares for the next process to run.

	*** Interupt-Event Driven Multi-tasking ***

	Since only one processor can execute a process at a time, the OS uses MULTITASKING and PROCESS VIRTUALIZATION to create the 
	illusion that multiple processes are being executed concurrently by dedicated processors. This is achieved by rapidly switching 
	the CPU between processes, where the OS must ensure fair execution and efficient utilization of system resources. This 'context-switching'
	is handled through an Interrupt-Event Cycle, which goes as follows:

	1) Interrupt Signal Received:
		The CPU detects an interrupt signal, which could originate from hardware, software, or the operating system's timer,
		indicating the need for immediate attention.

		The current program counter (the address of the next instruction in the running process) is stored in a stack to preserve the state of the interrupted process.

	3) Interrupt Handler Invoked:
		The interrupt signal triggers the execution of an Interrupt Service Routine (ISR) or Interrupt Handler, a dedicated routine
		designed to address the specific type of interrupt received.

	4) Process State Saved:
		The interrupt handler saves the complete state of the interrupted process (including CPU registers and memory state)
		to ensure it can resume execution later without loss of data or functionality.

	5) Handler Executes Required Tasks:
		The handler performs the necessary actions to address the interrupt, such as processing I/O requests,
		handling hardware events, or managing system-level operations.

	6) Scheduler Called:
		Once the handler completes its tasks, it invokes the scheduler, which determines which process should execute next based 
		on the system's scheduling algorithm (e.g., priority, round-robin, etc.).

	7) Next Process Selected and Resumed:

		The scheduler selects the next process to run by considering system policies and current process states.
		The saved state of the selected process is restored (retrieving its program counter and registers).
		The CPU resumes execution of the newly selected process, ensuring smooth multitasking.
		This detailed process ensures that the CPU can efficiently switch between tasks, providing the foundation for pre-emptive multitasking.

`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
*/