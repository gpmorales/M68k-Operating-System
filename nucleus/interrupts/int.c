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

    Additionally, each device has an associated Device Register (devreg_t) that holds the
    opreation regster number, as well as the address, amount, and track or sector number. 
    It also has a status register code (device completion codes 0-9).

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


state_t* TERMINAL_INTERRUPT_OLD_AREA;
state_t* TERMINAL_INTERRUPT_NEW_AREA;
state_t* PRINTER_INTERRUPT_OLD_AREA;
state_t* PRINTER_INTERRUPT_NEW_AREA;


/*
    This function loads several entries in the EVT, it sets the new areas for the interrupts,
    and it defines the locations of the device registers.
*/
intinit()
{
 
}

waitforpclock(old)
state_t *old;
{
}

waitforio(old)
state_t *old;
{
}

intdeadlock()
{
print("halt: end of program");
/*
asm("trap #4");
*/
  HALT();
}

intschedule()
{
}
