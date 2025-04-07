/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../../h/const.h"
#include "../../h/types.h"
#include "../../h/procq.e"
#include "../../h/asl.e"


/*
	This module handles some of the support level system calls.It has the following
	functions: readfromterminal(), writetoterminal(), delay(), gettimeofday() and terminate().
*/


/*
	Requests that the invoking T-process be suspended until a line of input has been
	read from the associated terminal. The data read should be placed in the virtual
	memory of the T-process executing the SYS9, at the (virtual) address given in D3 at
	the time of the call. The count of the number of characters actually read from the
	terminal should be available in D2 when the T-process is continued. An attempt to
	read when there is no more data available should return the negative of the ‘‘End of
	Input’’ status code in D2. If the operation ends with a status other than ‘‘Successful
	Completion’’ or ‘‘End of Input’’ (as described above), the negative of the Status
	register value should be returned in D2. Any negative numbers returned in D2 are ‘‘error flags.’’
*/
void readfromterminal()
{

}


/*
	Requests that the T-process be suspended until a line (string of characters) of output
	has been written on the terminal associated with the process. The virtual address of
	the first character of the line to be written will be in D3 at the time of the call. 
	The count of the number of characters to be written will be in D4. The count of the
	number of characters actually written should be placed in D2 upon completion of
	the SYS10. As in SYS9, a non-successful completion status will cause an error flag
	to be returned instead of the character count.
*/
void writetoterminal()
{

}


/*
	On entry, D4 contains the number of microseconds for which the invoker is to be
	delayed. The caller is to be delayed at least this long, and not substantially longer.
	Since the nucleus controls low-level scheduling decisions, all that you can ensure is
	that the invoker is not dispatchable until the time has elapsed, but becomes dispatchable shortly thereafter.
*/
void delay()
{

}


/*
	Returns the value of the time-of-day clock in D2.
*/
void gettimeofday()
{
	// Interrupted T-Process state is saved in old_state (SYS)
	state_t* SYS_TRAP_OLD_STATE = (state_t*)0x930;

	// Get and time of day
	long timeOfDay;
	STCK(&timeOfDay);

	// Return value in D2
	SYS_TRAP_OLD_STATE->s_r[2] = timeOfDay;
}


/*
	Terminates the T-process. When all T-processes have terminated, your operating
	system should shut down. Thus, somehow the ‘system’ processes created in the
	support level must be terminated after all five T-processes have terminated. Since
	there should then be no dispatchable or blocked processes, the nucleus will halt.
*/
void terminate()
{

}
