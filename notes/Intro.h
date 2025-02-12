/*
`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
	<--- Chapter 1 --->

	The HOCA Operating System is designed to implemented in 3 Phases:

		- Phase 1 will implement two modules that are later used in the nucleas to
		  to implement the process queue and the semaphore abstractions (Mutexes for Processes)

		- Phase 2 will involve building the nucleus -> the routines that implement the
		  the notion of asynchronous (spawn a separate process) sequential processes, a pseduo-clock
		  and the synchronization primitives

		- Phase 3 will invole implementaing another software level known as the support level
		  which creates an environment with virtual memory, input/output devices and virtual
		  synchronization operations that are suitable for the exeuction of user programs


	<--- Chapter 2 --->

	HOCA is implemented as a set of C modules
	A module in C is made up of the following:
		- ".h" - Header files
		- ".c" - C source code
		- ".e" - Export files

	For each Module X, you should create three files: X.h, X.e, X.c where X.e and X.c should include X.h

	*** Header files contain contants, type definitions, and macros (#define)

	*** Export files contain contants, macros, types, variables, and function prototypes that are the module
		definitions visible to other modules.

	*** Constants, macros, types, and variables that are local to module X and that are not used by any other module
		should be declared at the top of the file X.c

	Example head.h:
		#define TRUE 1
		#define MAXPROC 20

		typedef struct vars {
			int i;
			int* j;
		} Variables;

	Example export.e:
		#include "head.h"
		extern int function1();
		extern int function2();

		NOTE:
		'Variables', 'TRUE', and 'MAXPROC' will all be visible in modules that include export.e because export.e itself includes head.h.
		The preprocessor expands #include "head.h" in export.e, making the macros (TRUE and MAXPROC) and the typedef (Variables) available to any module that includes export.e.
		In short, anything included in head.h is transitively included wherever export.e is included.

`````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````
*/
