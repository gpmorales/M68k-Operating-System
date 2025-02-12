/*
`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
	<--- Semaphores Overview --->

	A semaphore is a synchronization primitive used in concurrent programming to manage access to shared resources by multiple processes (or threads).
	It is essentially an integer value that is manipulated atomically through two primary operations:
	- Wait (P): Decrements the semaphore. If the value becomes negative, the calling process/thread is blocked until the semaphore is incremented.
	- Signal (V): Increments the semaphore. If there are any processes/threads waiting, one of them is unblocked.
	

	When the Semaphore Value is 0:
		- Indicates that the resource is currently unavailable.
		- Any thread or process that attempts to perform a P (Wait) operation will be blocked until the 
		  semaphore value is incremented (via a V/Signal operation).

	When the Semaphore Value is Greater than 0:
		- Indicates the availability of resources.
		- The value represents the number of units of the resource that are available.

	When the Semaphore Value is Less than 0 ****:
		- Indicates the number of processes/threads that are waiting for the semaphore to become available.
		- This happens in counting semaphores when more P operations occur than V operations.



	<--- Operating System Role and Active Semaphore --->

	As mentioned in my Processes notes, the Kernel will manage the lifecycle of processes 
	including the stage where a process is blocked and place in the according Blocked Queue.
	Now in our HOCA implementation we will use an ASL or Active Semaphore List.

	We consider a semaphore active if there is AT LEAST 1 process blocked on it which means 
	that for a counting Semaphore, its value is -1. (SEE ****)

	By having a singular list of sempahores, the ASL will efficiently track only the active 
	semaphores as these processes will be in a blocked state and the Kernel will then have
	to place them in the waiting queue after they are unblocked.

	Our HOCA implementation will follow these guidelines:
		- A doubly linked list of type semaphore descriptor
		- The semaphore address (just a primitive of type int)
		- A proc_link struct which will act as the pointer to the tail of the queue
		  which holds all processes blocked by this specific semaphore

	The head of the active semaphore list is pointed to by the variable semd_h.
	The elements in this list come from the array semdTable[MAXPROC] of type
	semd_t (defined in the introduction). 
	
	Additionally, the unused elements of the array semdTable will be kept on a
	ENULL-terminated free list headed by the variable semdFree_h.

`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
*/