/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/

#include "stdio.h"

/*
	This module coordinates the initialization of the nucleus and it starts
	the execution of the first process, p1(). It also provides a scheduling
	function. The module contains three routines:

		- void main()
		This function calls init(), sets up the processor state [state_t] for p1(),
		adds p1() to the Ready Queue and calls the schedule() routine

		- void static init():
		This function determines how much phyiscal memory there is in the system. It
		then calls initProc(), initSemd(), trapinit(), and intinit()

		- void schedule()

*/