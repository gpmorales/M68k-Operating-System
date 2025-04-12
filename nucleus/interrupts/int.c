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
#define QUANTUM 5000;

typedef struct {
    int status;
    int length;
} completion_stat;

/* Device related registers and semaphores */
int deviceSemaphores[TOTAL_DEVICES];
completion_stat deviceCompletionStats[TOTAL_DEVICES];
devreg_t* deviceRegisters[TOTAL_DEVICES];

/* Global Variables */
int PSEUDO_CLOCK = 0;               // Clock to track total time in milliseconds CPU has been active
int PSEUDO_CLOCK_SEMAPHORE = 0;     // No free resources

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

/* Misc routines */
void myprint(char*);

/* Interrupt Handlers */
void static intterminalhandler();
void static intprinterhandler();
void static intdiskhandler();
void static intfloppyhandler();
void static intclockhandler();


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
    long INTERVAL_SLICE = QUANTUM;
    LDIT(&INTERVAL_SLICE);
}


/*
    This function is similar to the semop call in the first part. It has two arguments,
    the address of a semaphore (instead of a state_t), and the operation. 
    This function should use the ASL and should call insertBlocked and removeBlocked
    Note that in our implementation, a device can ONLY block 1 process at any given moment
*/
void static intsemop(int* semAddr, int op)
{
    // This function is invoked by 
    // - wait_for_io (to BLOCK the running process for I/O operations)
    // - wait_for_pclock (to BLOCK the running process on the pseudo clock)
    // - inthandler (to UNBLOCK the process that requested the I/O operation)
    // - intclockhandler (to UNBLOCK the processes on the pseudo-clock)

    // Get the semaphore proper and update the semaphore
    int prevSemVal = *semAddr;
    *semAddr = prevSemVal + op;	    

    // Note that wait_for_io and wait_for_plock are blocking the interrupted process at the head of the RQ
    if (op == LOCK) {
        // Ensure the semaphore has no more free resources before blocking the process
        if (prevSemVal <= 0) {
            // Remove the interrupted process from the RQ
            proc_t* process = removeProc(&readyQueue);

            // Semaphore has become negative, meaning it should block the process that invoked the wait_for_io or wait_for_plock routines
            insertBlocked(semAddr, process);

            // This process is no longer running, prime interval timer and prepare to run next process on RQ
            schedule();
        }
    }
    // Note that inthandler and intclockhandler are unblocking process that requested I/O operations OR sleeping processes
    else if (op == UNLOCK) {
        if (prevSemVal < 0) {
            // Remove the process at the head of the corresponding device or pseudo-clock Semaphore Queue
            proc_t* process = removeBlocked(semAddr);

            // If the process is no longer blocked on any other semaphores, then add it back to the RQ
            if (process != (proc_t*)ENULL && process->qcount == 0) {
                insertProc(&readyQueue, process);
            }
        }
    }
}


/*
    This function does an intsemop(LOCK) on a global variable called pseudoclock.
*/
void waitforpclock()
{
    // Grab the process requesting to block itself on the pseudoclock
    proc_t* process = headQueue(readyQueue);

    // Grab the interrupted process's state 
    state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Update the process's current processor state as it will be blocked and its state will need to be reloaded later
    process->p_s = *SYS_TRAP_OLD_STATE;

    // Perform the LOCK operation on the pseudo-clock and switch execution flow
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
    // Grab the process initiating the I/O operation
    proc_t* process = headQueue(readyQueue);

    // Get the absolute device number for which this I/O request
    state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;
    int deviceNumber = SYS_TRAP_OLD_STATE->s_r[4];

    // In the case where the device interrupt has NOT occured, BLOCK on that device's semaphore until we recieve the interrupt (V op)
    if (deviceSemaphores[deviceNumber] <= 0) {
		// Update the process's current processor state as it will be blocked and its state will need to be reloaded later
		process->p_s = *SYS_TRAP_OLD_STATE;
        intsemop(&deviceSemaphores[deviceNumber], LOCK);
    }
    // Otherwise the interrupt has already occured which happens on a V (+1) operation, so this semahpore's value is 1
    else {
        // At this point, inthandler will have already been invoked and has incremented the semaphore value ONLY
        SYS_TRAP_OLD_STATE->s_r[2] = deviceCompletionStats[deviceNumber].length;
        SYS_TRAP_OLD_STATE->s_r[3] = deviceCompletionStats[deviceNumber].status;

        // Decrement (P) that devices semaphore as the device operation has already been completed, resuming this process's execution via LDST in trap.c
        deviceSemaphores[deviceNumber]--; 
    }
}


/*
    This function saves the completion status if a wait_for_io call has not been received,
    or it does an intsemop(UNLOCK) on the semaphore corresponding to that device.
*/
void static inthandler(int deviceIndex)
{
    // Since the process that initiated the I/O might be blocked and not currently running, we need to save
    // the device's completion status (operation status and length for printer/terminal devices) separately
    // The saved completion status will be accessed later when the original process that requested the I/O resumes

    // The wait_for_io call was made previously by if the device's semaphore is -1, as only calling the wait_for_io could have blocked, hence this interrupt was eventually expected
    if (deviceSemaphores[deviceIndex] == -1) {
        // Return status and length if applicable
        proc_t* process = headBlocked(&deviceSemaphores[deviceIndex]);

        // The device registers are stored in memory, accessible and indexable with our deviceRegisters arr
        // wait-for-io also stores these values, return them
		process->p_s.s_r[2] = deviceRegisters[deviceIndex]->d_dadd; // length
        process->p_s.s_r[3] = deviceRegisters[deviceIndex]->d_stat; // status

        // Unblock the waiting process by performing a V (+1) operation
        intsemop(&deviceSemaphores[deviceIndex], UNLOCK);
    }
    // Otherwise the interrupt occurs before the process has a chance to invoke wait_for_io
    else {
        // The device’s Status register constitute the I/O operation completion status, and has already been stored by nucleus (deviceRegisters). 
        // Update this devices completion stats's Status and Length register so we can return it in waitforio
        deviceCompletionStats[deviceIndex].status = deviceRegisters[deviceIndex]->d_stat;
		deviceCompletionStats[deviceIndex].length = deviceRegisters[deviceIndex]->d_dadd;

        // Increment the semaphore value to indicate the interrupt already occured
        deviceSemaphores[deviceIndex]++;
    }
}


/*
    This function is called when the RQ is empty. If there are processes blocked on the pseudoclock, it calls intschedule() and it
    goes to sleep. If there are processes blocked on the I/O semaphores it goes to sleep. If there are no processes left it shuts
    down normally and it prints a normal termination message. Otherwise it prints a deadlock message.
*/
void intdeadlock()
{
    // One or more processes are blocked on the pseudo semaphore clock
    if (headBlocked(&PSEUDO_CLOCK_SEMAPHORE) != (proc_t*)ENULL) {
        // Call intschedule to prepare a timer interrupt to invoke intclockhandler to load the next process on the RQ
        intschedule();

		// Halt the CPU while we wait for this device's interrupt to occur 
		sleep();
    }

    // In case where we are waiting for devices to send an interrupt as indication for the completion of some operation
    int i;
    for (i = 0; i < TOTAL_DEVICES; i++) {
        // if any process is blocked on an I/O semaphore, sleep so the CPU can conserve resources
        if (headBlocked(&deviceSemaphores[i]) != (proc_t*)ENULL) {
			// Halt the CPU while we wait for this device's interrupt to occur 
            sleep();
        }
    }

    // If we reach this point, this means there are no process blocked on I/O semaphores OR on the pseudo-clock semaphore
    // Check if there are any other process blocked by any other normal Semaphores (ASL list is empty meaning the CPU has executed all processes)
    if (!headASL()) {
        myprint("nucleus: normal termination");
        HALT();
    }
    else {
        myprint("nucleus: deadlock termination");
        HALT();
    }
}


/*
    If the RQ is not empty, this function removes the process at the head of the queue and it then adds it to 
    the tail of the queue. This function does an intsemop(UNLOCK) on the pseudoclock semaphore if
    necessary and then it calls schedule() to begin the execution of the process at the head of the RQ.
*/
void static intclockhandler()
{
    // Grab the process running on the CPU
    proc_t* process = headQueue(readyQueue);

    // Only the hardware timer can generate clock interrupts. Since every interrupt should happen a qunatum apart, add it to the pseudo-clock
    PSEUDO_CLOCK += QUANTUM;

    // Perform Round robin, remove process at head and add it to tail of RQ
    if (process != (proc_t*)ENULL) {
		// Update the running process's state before we load next process on CPU
        removeProc(&readyQueue);
        updateTotalTimeOnProcessor(process);
		process->p_s = *CLOCK_INTERRUPT_OLD_STATE;
        insertProc(&readyQueue, process);
    }

	// Check how much time has passed on the pseudo-clock and unblock the first sleeping process on the pseudo clock semaphore if possible
    if (PSEUDO_CLOCK >= 100000) {
        if (headBlocked(&PSEUDO_CLOCK_SEMAPHORE) != (proc_t*)ENULL) {
            intsemop(&PSEUDO_CLOCK_SEMAPHORE, UNLOCK);
        }
        PSEUDO_CLOCK = 0;
    }

    // This call primes the Interval Timer to throw an interrupt when the RQ is NOT empty, triggering a round robin pre-emption cycle
	schedule();
}


/*
    Interrupt Request Handlers (these routines pass the device type and the device number to inthandler)
*/
void static intterminalhandler() 
{
    // Grab the interrupt's corresponding device information stored in the appropiate interrupt area
    tmp_t tempStorage = TERM_INTERRUPT_OLD_STATE->s_tmp;
    int deviceNumber = tempStorage.tmp_int.in_dno;

    // Grab the current process on the RQ when the interrupt occured
    proc_t* process = headQueue(readyQueue);

    // The generic interrupt handler will perform an unlock operation on this device's semaphore to indicate that it finished an operation
    // This adds the process that was blocked on this device's IO resource back to the RQ
    inthandler(deviceNumber);

	// If the RQ was not empty when the interrupt occured, continue executing the current process
    if (process != (proc_t*)ENULL) {
        LDST(TERM_INTERRUPT_OLD_STATE);
    }
    // Otherwise we call schedule to prime the Interval Timer and load the next process on the RQ or deadlock
    else {
        schedule();
    }
}


void static intprinterhandler()
{
    // Grab the interrupt's corresponding device information stored in the appropiate interrupt area
    tmp_t tempStorage = PRINTER_INTERRUPT_OLD_STATE->s_tmp;
    int deviceNumber = tempStorage.tmp_int.in_dno;          // relative device number

    // Grab the current process on the RQ when the interrupt occured
    proc_t* process = headQueue(readyQueue);

    // The generic interrupt handler will perform an unlock operation on this device's semaphore to indicate that it finished an operation
    // This adds the process that was blocked on this device's IO resource back to the RQ
    inthandler(deviceNumber + 5);

	// If the RQ was not empty when the interrupt occured, continue executing the current process
    if (process != (proc_t*)ENULL) {
        LDST(PRINTER_INTERRUPT_OLD_STATE);
    }
    // Otherwise we call schedule to prime the Interval Timer and load the next process on the RQ or deadlock
    else {
        schedule();
    }
}


void static intdiskhandler()
{
    // Grab the interrupt's corresponding device information stored in the appropiate interrupt area
    tmp_t tempStorage = DISK_INTERRUPT_OLD_STATE->s_tmp;
    int deviceNumber = tempStorage.tmp_int.in_dno;          // relative device number

    // Grab the current process on the RQ when the interrupt occured
    proc_t* process = headQueue(readyQueue);

    // The generic interrupt handler will perform an unlock operation on this device's semaphore to indicate that it finished an operation
    // This adds the process that was blocked on this device's IO resource back to the RQ
    inthandler(deviceNumber + 7);

	// If the RQ was not empty when the interrupt occured, continue executing the current process
    if (process != (proc_t*)ENULL) {
        LDST(DISK_INTERRUPT_OLD_STATE);
    }
    // Otherwise we call schedule to prime the Interval Timer and load the next process on the RQ or deadlock
    else {
        schedule();
    }
}


void static intfloppyhandler()
{
    // Grab the interrupt's corresponding device information stored in the appropiate interrupt area
    tmp_t tempStorage = FLOPPY_INTERRUPT_OLD_STATE->s_tmp;
    int deviceNumber = tempStorage.tmp_int.in_dno;

    // Grab the current process on the RQ when the interrupt occured
    proc_t* process = headQueue(readyQueue);

    // The generic interrupt handler will perform an unlock operation on this device's semaphore to indicate that it finished an operation
    // This adds the process that was blocked on this device's IO resource back to the RQ
    inthandler(deviceNumber + 11);

	// If the RQ was not empty when the interrupt occured, continue executing the current process
    if (process != (proc_t*)ENULL) {
        LDST(FLOPPY_INTERRUPT_OLD_STATE);
    }
    // Otherwise we call schedule to prime the Interval Timer and load the next process on the RQ or deadlock
    else {
        schedule();
    }
}


/*
    My print function will write to printer 0
*/
void myprint(char* msg)
{
    devreg_t* printer0 = deviceRegisters[5];     // Get printer0's device register from memory
    printer0->d_stat = DEVNOTREADY;              // Set status code to non 0 value as we prepare to request I/O
    printer0->d_dadd = strlen(msg);              // For printer, the length of data
    printer0->d_badd = msg;                      // Buffer address has the pointer to the data we are handing to device
    printer0->d_op = IOWRITE;                    // Set the device operation status to WRITE

    // 'Block' until operation is done
    while (printer0->d_stat != NORMAL);
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
        // Each devreg_t is 16 bytes long (0x10 apart)
        deviceRegisters[i] = (devreg_t*)BEGINDEVREG + i;
        deviceSemaphores[i] = 0;
    }

    // Allocate New and Old State Areas for Device Interrupts
    TERM_INTERRUPT_OLD_STATE = (state_t*)BEGININTR;
    state_t* TERM_INTERRUPT_NEW_STATE = (state_t*)TERM_INTERRUPT_OLD_STATE + 1;
    TERM_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    TERM_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                        // Set Supervisor Bit
    TERM_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                      // Interrupt Priority disabled
    TERM_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
    TERM_INTERRUPT_NEW_STATE->s_pc = (int)intterminalhandler;	                    // The address for this device's specific handler

    PRINTER_INTERRUPT_OLD_STATE = (state_t*)0xa60;
    state_t* PRINTER_INTERRUPT_NEW_STATE = (state_t*)PRINTER_INTERRUPT_OLD_STATE+ 1;
    PRINTER_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    PRINTER_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
    PRINTER_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                   // Interrupt Priority disabled
    PRINTER_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                // Set the global stack pointer to the top, where the Kernel memory is allocated
    PRINTER_INTERRUPT_NEW_STATE->s_pc = (int)intprinterhandler;    	                // The address for this specific handler

    DISK_INTERRUPT_OLD_STATE= (state_t*)0xaf8;
    state_t* DISK_INTERRUPT_NEW_STATE = (state_t*)DISK_INTERRUPT_OLD_STATE + 1;
    DISK_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    DISK_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                        // Set Supervisor Bit
    DISK_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                      // Interrupt Priority disabled
    DISK_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
    DISK_INTERRUPT_NEW_STATE->s_pc = (int)intdiskhandler;       	                // The address for this specific handler

    FLOPPY_INTERRUPT_OLD_STATE= (state_t*)0xb90;
    state_t* FLOPPY_INTERRUPT_NEW_STATE= (state_t*)FLOPPY_INTERRUPT_OLD_STATE + 1;
    FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
    FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                    // Interrupt Priority disabled
    FLOPPY_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                // Set the global stack pointer to the top, where the Kernel memory is allocated
    FLOPPY_INTERRUPT_NEW_STATE->s_pc = (int)intfloppyhandler;	                    // The address for this specific handler

    CLOCK_INTERRUPT_OLD_STATE= (state_t*)0xc28;
    state_t* CLOCK_INTERRUPT_NEW_STATE = (state_t*)CLOCK_INTERRUPT_OLD_STATE + 1;
    CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
    CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_int = 7;                                     // Interrupt Priority disabled
    CLOCK_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
    CLOCK_INTERRUPT_NEW_STATE->s_pc = (int)intclockhandler;		                    // The address for this specific handler
}
