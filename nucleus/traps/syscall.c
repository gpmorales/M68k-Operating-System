#include "../../h/types.h"				

/*
	Definition of System call routines SYS1, SYS2, SYS3, SYS4, SYS5 and SYS6.
	These are executable only by processes running in supervisor mode. 
	If invoked in usermode, a privileged instruction trap should be generated.
*/

void creatproc(state_t* old_state)
{

}

void killproc(state_t* old_state) 
{

}

void semop(state_t* old_state)
{

}

void notused(state_t* old_state) {

}


void trapstate(state_t* old_state)
{

}

void getcputime(state_t* old_state)
{

}