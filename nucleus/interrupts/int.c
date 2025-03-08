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
int PSEUDO_CLOCK;
extern int MEMSTART;
extern proc_link readyQueue;

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
void static intsemop(int* semdAddr, int operation)
{

}


/*
	This function does an intsemop(LOCK) on a global variable called pseudoclock.
*/
void waitforpclock()
{
    // Grab the interrupted process
    proc_t* process = headQueue(readyQueue);

    // Grab the interrupted process's state and registers
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

    // Update the process's current processor state
    process->p_s = *SYS_TRAP_OLD_STATE;

    // LOCK the pseudoclock
    intsemop(&PSEUDO_CLOCK, LOCK);
}


/*
	This function first checks if the interrupt already occurred. If it has, it decrements the semaphore
    and passes the completion status to the process.Otherwise this function does an intsemop(LOCK) on the semaphore corresponding to each device.
*/
void waitforio()
{
    // Grab the interrupted process
    proc_t* process = headQueue(readyQueue);

    // Grab the interrupted process's state and registers
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

    // Update the process's current processor state
    process->p_s = *SYS_TRAP_OLD_STATE;

    // Check if interrupt has already occured
}


/*
	This function is called when the RQ is empty.If there are processes blocked on the pseudoclock, it calls intschedule() and it
	goes to sleep. If there are processes blocked on the I / O semaphores it goes to sleep. If there are no processes left it shuts
	down normally and it prints a normal termination message.Otherwise it prints a deadlock message
*/
void intdeadlock()
{
	print("halt: end of program");
	/*
	asm("trap #4");
	*/
	HALT();
}


/*
	This functions simply loads the timeslice into the Interval Timer.
*/
void intschedule()
{
    // The clock will send an interrupt every X milliseconds to load a new program on the CPU
    long TIME_SLICE_INTERVAL;
    LDIT(&TIME_SLICE_INTERVAL);
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

}


/*
	This function saves the completion status if a wait_for_io call has not been received,
    or it does an intsemop(UNLOCK) on the semaphore corresponding to that device.
*/
void static inthandler()
{

}


/*
    This function is called when the RQ is empty.This function could enable interrupts and enter
    an infinite loop, or it could execute the "stop" assembly instruction. From C call the asm("stop #0x2000") instruction
    which loads 0x2000 into the status register, i.e. it enables interrupts and sets supervisor mode, and then
	the CPU halts until an interrupt occurs. The simulator runs much faster if stop is used.
*/
void static sleep()
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
    // Initialize the Pseudo Clock
    PSEUDO_CLOCK = 0;

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
    TERM_INTERRUPT_NEW_STATE->s_sr.ps_int = 0;                                      // Interrupt priority level 0 for Terminal Devices
	TERM_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                        // Set Supervisor Bit
	TERM_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
	TERM_INTERRUPT_NEW_STATE->s_pc = (int)intterminalhandler;	                    // The address for this device's specific handler

    PRINTER_INTERRUPT_OLD_STATE = (state_t*)0xa60;
    state_t* PRINTER_INTERRUPT_NEW_STATE = (state_t*)PRINTER_INTERRUPT_OLD_STATE+ 1;
	PRINTER_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    PRINTER_INTERRUPT_OLD_STATE->s_sr.ps_int = 1;                                   // Interrupt priority level 1 for Printer Devices
	PRINTER_INTERRUPT_OLD_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
	PRINTER_INTERRUPT_OLD_STATE->s_sp = MEMSTART;					                // Set the global stack pointer to the top, where the Kernel memory is allocated
	PRINTER_INTERRUPT_OLD_STATE->s_pc = (int)intprinterhandler;    	                // The address for this specific handler

    DISK_INTERRUPT_OLD_STATE= (state_t*)0xaf8;
    state_t* DISK_INTERRUPT_NEW_STATE = (state_t*)DISK_INTERRUPT_OLD_STATE + 1;
	DISK_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    DISK_INTERRUPT_NEW_STATE->s_sr.ps_int = 2;                                      // Interrupt priority level 2 for Disk Devices
	DISK_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                        // Set Supervisor Bit
	DISK_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
	DISK_INTERRUPT_NEW_STATE->s_pc = (int)intdiskhandler;       	                // The address for this specific handler

    FLOPPY_INTERRUPT_OLD_STATE= (state_t*)0xb90;
    state_t* FLOPPY_INTERRUPT_NEW_STATE= (state_t*)FLOPPY_INTERRUPT_OLD_STATE + 1;
	FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_int = 3;                                    // Interrupt priority level 3 for Floppy Devices
	FLOPPY_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
	FLOPPY_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                // Set the global stack pointer to the top, where the Kernel memory is allocated
	FLOPPY_INTERRUPT_NEW_STATE->s_pc = (int)intfloppyhandler;	                    // The address for this specific handler

    CLOCK_INTERRUPT_OLD_STATE= (state_t*)0xc28;
    state_t* CLOCK_INTERRUPT_NEW_STATE = (state_t*)CLOCK_INTERRUPT_OLD_STATE + 1;
	CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_m = 0;	 				                    // Set memory management to physical addressing (no process virtualization)
    CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_int = 5;                                     // Interrupt priority level 5 for Clock Devices
	CLOCK_INTERRUPT_NEW_STATE->s_sr.ps_s = 1;   				                    // Set Supervisor Bit
	CLOCK_INTERRUPT_NEW_STATE->s_sp = MEMSTART;					                    // Set the global stack pointer to the top, where the Kernel memory is allocated
	CLOCK_INTERRUPT_NEW_STATE->s_pc = (int)intclockhandler;		                    // The address for this specific handler
}
