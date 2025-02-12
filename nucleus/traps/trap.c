/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/

#include "stdio.h"

/*
	This module handles the traps, it has the following static functions:
	trapinit(), trap-syshandler(), trapmmhandler(), and trapproghandler()

		- void trapinit(): 
		Populates the Exception Vector Table entries and allocates memory for each exception handler, storing their addresses in the EVT.
		When traps occur, the control flow is transferred via the Trap State Vector which saves the old processor state and 
		loads the new processor state's address from the EVT.

		- void static trapsyshandler():
		This function handles 9 different traps. It has a switch statment and each case calls a function.
		Two of the functions, waitforpclock() and waitforio() are in int.c The other secen are in syscall.c

			NOTE: During init() I will map EVT trap numbers (32-47) to their corresponding SYS functions. 
			The tmp_sys.sys_no field will hold that trap number, letting the kernel know which SYS function (SYS1-SYS7) to execute.


		- void static trapmmhandler():
		- void static trapproghandler():
		- Thes functions will pass up memory management and program traps the process.

*/