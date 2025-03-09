/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/

#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/vpop.h"
#include "../../h/procq.e"
#include "../../h/asl.e"


/*
    Just like a trap, an interrupt is a signal that changes the execution flow of the CPU.
    In our implementation, an interrupt is signaled with an interrupt request. There are 8 interrupt
    priority levels (0-7). Each device is associated with one of these levels at System configuration
    time. 0 or more devices can be associated with the same priority lvl. Among these interrupt requests
    the one with the highest priority is served first.

    For each priority level, asm routines are provided which define an interrupt area starting at
    a predefined location in memory, just like with traps. There are 5 types of devices

        - Terminal  (priority - 0) [at most 5 terminals]
        - Printer   (prioirty - 1) [at most 2 printers]
        - Disk      (priority - 2) [at most 4 disks]
        - Floppy    (priority - 3) [at most 4 floppy]
        - Clock     (priority - 5) [at most 1 clock]

    For each device (numbered 0-14), the Exeception Vector Table has an entry  at 0x100 + (device num * 4).
    This entry holds the address of the appropiate interrupt handler routine for that specific device.

    For example, for a Clock interrupt, we use the routine at address 0x100 + (16 * 4) = 0x140 (STDCLOCK see trapinit)
    For an interrupte from device TERM0, we use the interrupt handler address is located at 0x100 + (0 * 4)
    Likely, devices of the same type will share the same routine handler, though each will have a unique EVT entry

    The Interrupt Area Address are as follows (note that there are 152 bytes between each ares, 2 * state_t structs for OLD and NEW states)
        - Terminal   [0x9c8]
        - Printer    [0xa60]
        - Disk       [0xaf8]
        - Floppy     [0xb90]
        - Clock      [0xc28]

    Additionally, each device has an associated Device Register (devreg_t) that holds the opreation regster number,
    as well as the address, amount, and track or sector number. It also has a status register code (device completion codes 0-9).

    For the devices, we can access there Device Register in memory using the following formulas:
    *----------------------------------------------------*
    | Device           | Starting Address   | Range of i |
    *----------------------------------------------------*
    | Terminal(i)      |  0x1400 + 0x10*i   |  0<=i<=4   |
    | Printer(i)       |  0x1450 + 0x10*i   |  0<=i<=1   |
    | Disk(i)          |  0x1470 + 0x10*i   |  0<=i<=3   |
    | Floppy(i)        |  0x14b0 + 0x10*i   |  0<=i<=3   |
    *----------------------------------------------------*

*/

#define TOTAL_DEVICES 15

/* Device related registers and semaphores */
devreg_t* deviceRegisters[TOTAL_DEVICES];
int* deviceSemaphores[TOTAL_DEVICES];

/* Global Variables */
int PSEUDO_CLOCK = 0;               // Clock to track total time in milliseconds CPU has been active
int PSEUDO_CLOCK_SEMAPHORE = 0;     // No free resources
long TIME_SLICE_INTERVAL = 5000;    // Quantum of 5 milliseconds

extern int MEMSTART;
extern proc_link readyQueue;
extern void schedule();

/* Interrupt Area States */
state_t* TERM_INTERRUPT_OLD_STATE;
state_t* PRINTER_INTERRUPT_OLD_STATE;
state_t* DISK_INTERRUPT_OLD_STATE;
state_t* FLOPPY_INTERRUPT_OLD_STATE;
state_t* CLOCK_INTERRUPT_OLD_STATE;

/* SYS Calls 7 & 8 */
void waitforpclock();
void waitforio();

/* CPU starvation handler */
void intdeadlock();

/* CPU multiplexing handler */
void intschedule();

/* Interrupt Handlers */
void static intterminalhandler();
void static intprinterhandler();
void static intdiskhandler();
void static intfloppyhandler();
void static intclockhandler();


/*
	This function is similar to the semop call in the first part. It has two arguments,
    the address of a semaphore (instead of a state_t), and the operation. 
    This function should use the ASL and should call insertBlocked and removeBlocked
*/
void static intsemop(int* semAddr, int op)
{
    // Grab the interrupted process
    proc_t* process = headQueue(readyQueue);

	int callingProcessBlocked = TRUE;

    // Grab the interrupted process's state 
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Get the semaphore proper and update the semaphore
	int prevSemVal = *semAddr;
	*semAddr = prevSemVal + op;	    

	// P (-1) will decrement the semaphore, if the sem value is negative afterwards, the interrupted process should be blocked
	if (op == LOCK) {
		if (prevSemVal <= 0) {
			// Semaphore has become negative, meaning its blocking at least the process and is now active
			// The running process at the head of the Queue can be blocked by a P operation 
			process->p_s = *SYS_TRAP_OLD_STATE;
			insertBlocked(semAddr, process);
            removeProc(&readyQueue);
            schedule();
		}
		else {
			// Do nothing if the semaphore still has resources
		}
	}
	// V (+1) on an active semaphore (has a negative value) means a resource has been freed, allowing the next blocked process on that Semaphore to maybe be put back on RQ
	else if (op == UNLOCK) {
		if (prevSemVal < 0) {
			// Remove the process at the head of the corresponding Semaphore Queue and update Semvec
			proc_t* process = removeBlocked(semAddr);

			// If the process is no longer blocked on any Semaphores, then add it back to the RQ
			if (process != (proc_t*)ENULL && process->qcount == 0) {
				insertProc(&readyQueue, process);
			}
		}
		else {
			// Do nothing if the semaphore was not active, as resources were already free
		}
	}
}


/*
	This function does an intsemop(LOCK) on a global variable called pseudoclock.
*/
void waitforpclock()
{
    // Grab the interrupted process
    proc_t* process = headQueue(readyQueue);

    // Grab the interrupted process's state 
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

    // Update the process's current processor state
    process->p_s = *SYS_TRAP_OLD_STATE;

    // Perform LOCK the pseudoclock
    intsemop(&PSEUDO_CLOCK_SEMAPHORE, LOCK);
}


/*
	This instruction performs a P operation on the semaphore that the nucleus maintains
	for the I/O device whose number is in D4. All possible devices for the EMACSIM are
	assigned unique numbers in the range 0-14 as shown in the const.h file of Appendix 2.
    The appropriate device semaphore is V’ed every time an interrupt is generated by the I/O device.

	Once the process resumes after the occurrence of the anticipated interrupt for printer
	and terminal devices, D2 should contain the contents of the Length register for the
	appropriate device and, for all devices, D3 should contain the contents of the
	device’s Status register. These two registers constitute the I/O operation completion
	status (see above), and may have already been stored by nucleus. This will occur in
	cases where an I/O interrupt occurs before the corresponding SYS8 instruction
*/
void waitforio()
{
    // Grab the interrupted process
    proc_t* process = headQueue(readyQueue);

    // Grab the interrupted process's state and registers
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

    // Update the process's current processor state
    process->p_s = *SYS_TRAP_OLD_STATE;

    // Check if interrupt has already occured. A V (+1) operation occurs the device generates an interrupt
    unsigned deviceNumber = SYS_TRAP_OLD_STATE->s_r[4];

    // Check whether the device has already generated an interrupt
    if (deviceSemaphores[deviceNumber] > 0) {
        // Pass the completion status to the process!!!! TODO

        // Decrement (P) the semaphore
        (*deviceSemaphores[deviceNumber])--;
    }
}


/*
    This function is called when the RQ is empty. This function could enable interrupts and enter
    an infinite loop, or it could execute the "stop" assembly instruction. From C call the asm("stop #0x2000") instruction
    which loads 0x2000 into the status register, i.e. it enables interrupts and sets supervisor mode, and then
	the CPU halts until an interrupt occurs. The simulator runs much faster if stop is used.
*/
void static sleep()
{
    asm("stop #0x2000");
}


/*
	This functions simply loads the timeslice into the Interval Timer.
    Note that when we load the timeslice, the interrupt is only fired once after the time interval passes.
*/
void intschedule()
{
    LDIT(&TIME_SLICE_INTERVAL);
}


/*
	This function is called when the RQ is empty. If there are processes blocked on the pseudoclock, it calls intschedule() and it
	goes to sleep. If there are processes blocked on the I/O semaphores it goes to sleep. If there are no processes left it shuts
	down normally and it prints a normal termination message. Otherwise it prints a deadlock message
*/
void intdeadlock()
{
    // One or more processes are blocked on the semaphore clock
    if (headBlocked(&PSEUDO_CLOCK_SEMAPHORE) != (proc_t*)ENULL) {
        // Call intschedule to prepare a timer interrupt to invoke intclockhandler to load the next process on the RQ
        intschedule();

        // Halt the CPU while we wait for this to timed interrupt to occur to load next process
        sleep();
    }

    // In case where we are waiting for devices to send an interrupt on completion of some task
    int i;
    for (i = 0; i < TOTAL_DEVICES; i++) {
        // if any process is blocked on an I/O semaphore, sleep so the CPU can conserve resources
        if (deviceSemaphores[i] < 0) {
            sleep();
            break; // TODO ? does not this 
        }
    }

    // If we reach this point, this means there are no process blocked on I/O semaphores OR on the pseudo-clock semaphore
    // Ideally, a process would have been added to RQ so the CPU resumes execution

    // TODO
    // Check if there are any other process blocked by any other Semaphores (ASL list is not empty)
    if (!headASL()) {
		HALT();
		printresult("Normal termination of CPU");
    }
    else {
		HALT();
		printresult("Deadlock occurred"); // ????
    }
}


/*
    Interrupt Request Handlers
*/
void static intterminalhandler() 
{

}


void static intprinterhandler()
{

}


void static intdiskhandler()
{

}


void static intfloppyhandler()
{

}


/*
    If the RQ is not empty, this function removes the process at the head of the queue and it then adds it to the tail of the queue.
    This function does an intsemop(UNLOCK) on the pseudoclock semaphore if necessary and then it calls schedule() to begin the execution of the process at the head of the RQ.
*/
void static intclockhandler()
{
    // Grab the running process at the head of the RQ
    proc_t* runningProcess = headQueue(readyQueue);

    // Perform Round robin, remove process at head and add to tail of RQ
    if (runningProcess != (proc_t*)ENULL) {
        removeProc(&readyQueue);
        insertProc(&readyQueue, runningProcess);

		// TODO Check how long the processes blocked by the Semaphore Pseudoclock have been blocked and 'wake them up' by readding them to RQ

		// Prepare to run next process in RQ for some time slice
		schedule();
    }
    else {
        intdeadlock();
    }
}


/*
	This function saves the completion status if a wait_for_io call has not been received,
    or it does an intsemop(UNLOCK) on the semaphore corresponding to that device.
*/
void static inthandler()
{

}


/*
    This function loads several entries in the EVT, it sets the new areas for the interrupts,
    and it defines the locations of the device registers.

	Since each semaphore is paried with a device to enable event-driven, asynchronous device functionaliyy, use an an 
    array of semaphores indexed by the device numbers listed in const.h to keep track of whether the device is blocking on while we wait for 
    an interrupt and some sort of completion status.

    Furthermore, the contents of the device’s Status register and, if appropriate, Length register, should be saved;
    this is the I/O operation’s completion status. Use an array also indexed by the device number to store the contents of these

*/
void intinit()
{
    // Load the device registers for each device
    int i;
    for (i = 0; i < TOTAL_DEVICES; i++) {
        // eaceh devreg_t is 16 bytes long (0x10 apart)
         deviceRegisters[i] = (devreg_t*)BEGINDEVREG + i;
         deviceSemaphores[i] = 0; 
    }

	// Allocate New and Old State Areas for Device Interrupts
    TERM_INTERRUPT_OLD_STATE = (state_t*)BEGININTR;
    state_t* TERM_INTERRUPT_NEW_STATE = (state_t*)TERM_INTERRUPT_OLD_STATE + 1;
	TERM_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
	TERM_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                        // Set Supervisor Bit
    TERM_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                      // Interrupt priority disabled
	TERM_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
	TERM_INTERRUPT_NEW_STATE->s_pc = (int)intterminalhandler;	                    // The address for this device's specific handler

    PRINTER_INTERRUPT_OLD_STATE = (state_t*)0xa60;
    state_t* PRINTER_INTERRUPT_NEW_STATE = (state_t*)PRINTER_INTERRUPT_OLD_STATE+ 1;
	PRINTER_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
	PRINTER_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
    PRINTER_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                   // Interrupt priority disabled
	PRINTER_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                // Set the global stack pointer to the top, where the Kernel memory is allocated
	PRINTER_INTERRUPT_NEW_STATE->s_pc = (int)intprinterhandler;    	                // The address for this specific handler

    DISK_INTERRUPT_OLD_STATE= (state_t*)0xaf8;
    state_t* DISK_INTERRUPT_NEW_STATE = (state_t*)DISK_INTERRUPT_OLD_STATE + 1;
	DISK_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
	DISK_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                        // Set Supervisor Bit
    DISK_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                      // Interrupt priority disabled
	DISK_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
	DISK_INTERRUPT_NEW_STATE->s_pc = (int)intdiskhandler;       	                // The address for this specific handler

    FLOPPY_INTERRUPT_OLD_STATE= (state_t*)0xb90;
    state_t* FLOPPY_INTERRUPT_NEW_STATE= (state_t*)FLOPPY_INTERRUPT_OLD_STATE + 1;
	FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
	FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
    FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                    // Interrupt priority disabled
	FLOPPY_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                // Set the global stack pointer to the top, where the Kernel memory is allocated
	FLOPPY_INTERRUPT_NEW_STATE->s_pc = (int)intfloppyhandler;	                    // The address for this specific handler

    CLOCK_INTERRUPT_OLD_STATE= (state_t*)0xc28;
    state_t* CLOCK_INTERRUPT_NEW_STATE = (state_t*)CLOCK_INTERRUPT_OLD_STATE + 1;
	CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
	CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
    CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                     // Interrupt priority disabled
	CLOCK_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
	CLOCK_INTERRUPT_NEW_STATE->s_pc = (int)intclockhandler;		                    // The address for this specific handler
}
