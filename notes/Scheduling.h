/*```````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````

*** Process Scheduling ***

Recall that the Kernel is the responsible component for mananging which processes are ran by the CPU and for how long.
This is known as process scheduling and is performed by what is known as a Scheduler, which decides the following:

	- Which processes to run next (based on scheduling algorithms like Round Robin, Shortest Job Next [SJN], Multi-Level Feeback Queue, etc.) 
	- How long a process gets the CPU before swtiching to another (time slicing in preemptive multitasking)
	- Handling context switches between processes
	- Managing process states (Ready, Running, Waiting, Terminated)

	Scheduling Algorithm:
	- Policy: Makes the decision of who gets to run
	Dispatcher:
	- Mechanism to do the context switch

````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````````


	<============= First Come, First Served (FCFS) =============>

	- Runs jobs to completion in the order they arrive

	=== SCENARIO A ===

	Process A, B, and C all arrive at the Run Queue at t = 0***
	Process A is chosen first and executes fully at t = 10
	Process B is chosen second and executes fully at t = 20
	Process C is chosen third and executes fully at t = 30
	
	 Proc C	Start Time
	      |
	      |   |--- Proc C End Time
	      v   v
	  A   B   C
	--0--10--20--30--40--50--60--70--80--90
	  ^
	  |
     Proc C Arrives

	*** The turnaround time here is the Time the process completes at (Tcomp) - Time it arrived at (Tarr) 
	Tturnarnd(A) = 10
	Tturnarnd(B) = 20
	Tturnarnd(C) = 30
	The average turn around time is (Total Turnaround Time / Number of Processes) = (10 + 20 + 30) / 3 = 20!!!


	=== SCENARIO B ===

	Process A, B, and C all arrive at the Run Queue at t = 0***
	Process A is chosen first and executes fully at t = 50
	Process B is chosen second and executes fully at t = 60
	Process C is chosen third and executes fully at t = 70
	
		          Proc C Start Time
				  |  
				  |   |--- Proc C End Time
				  v   v
	  A   A   A   A   A  AB  BC   C
	--0--10--20--30--40--50--60--70--80--90
	  ^
	  |
     Proc C Arrives

	*** In this case, process A has a long Turnaround time of 50
	Process B's turnaround time is 60
	Process C's turnaround time is 70
     	Despite the fact that Process B and C only needed 10 time units each to execute to completion
	This setup has an average setup time of (50 + 60 + 70) / 3 = 60!!!!
	



	<============= Shortest Job First (SJF) =============>
	
	=== SCENARIO B (See Above) ===

	Process A, B, and C all arrive at the Run Queue at t = 0***
	Process B is chosen first and executes fully at t = 10
	Process C is chosen second and executes fully at t = 20
	Process A is chosen third and executes fully at t = 70

         Proc C	Start Time
	      |  
	      |  |--- Proc C End Time
	      v  v
	  B  BC  CA   A   A   A   A   A
	--0--10--20--30--40--50--60--70--80--90
	  ^
	  |
     Proc C Arrives

	*** In this case we choose the shortest jobs to run first
	Process A's turnaround time is 70
	Process B's turnaround time is 10
	Process C's turnaround time is 20
     	Despite the fact that Process B and C only needed 10 time units each to execute to completion
	This setup has an average setup time of (70 + 10 + 20) / 3 = 33.3!!!!

	TAKEAWAY: This is much almost TWICE AS FAST as FCFS!!!!
	HOWEVER!!! If Process's B and C are placed in the Run Queue a little after Process A, then we are out of luck!




	<============= Analysis & Pre-emptive Schedulers =============>

	Both FCFS and SJF are what we refer to as NON-PREEMPTIVE SCHEDULERS	
	- This means that one job holds up all others!

	Let us Consider Response Tim
	- Response Time = delay before job starts = Trun - Tarrive

	=== SCENARIO A ===

		          Proc C Start Time
				  |  
				  |   |--- Proc C End Time
				  v   v
	  A   A   A   A   A  AB  BC   C
	--0--10--20--30--40--50--60--70--80--90
	  ^
	  |
     Proc C Arrives
	
	HERE the average response time is (0 for Proc A + 50 for Proc B + 60 for Proc C) / 3 = 36.67 [FCFS]


	=== SCENARIO B (See Above) ===

         Proc C	Start Time
	      |  
	      |  |--- Proc C End Time
	      v  v
	  B  BC  CA   A   A   A   A   A
	--0--10--20--30--40--50--60--70--80--90
	  ^
	  |
     Proc C Arrives

	HERE the average response time is (20 for Proc A + 0 for Proc B + 10 for Proc C) / 3 = 10 [FCFS]




	<============= Round Robin Algorithm & Pre-emptive Schedulers =============>

	Lets add preemption
	 - Let a job run for some time (we refer to this time slice as a QUANTUM)
	 - Then CONTEXT SWITCH and give someone else a turn


	Let the quantum be 2.5t (every -), in that case this is what the CPU would execute

	   ABCA  BCAB  CABC  ABCA  AAAA  AAAA  AAAA  
	--0----10----20----30----40----50----60----70----80----90
	
	Let us run the process's in this order: A, B, C
	At first we run Process A for 2.5t, then Process B for 2.5t, then Process B for 2.5t and then repeat

	The Average response time is (0 for Process A + 2.5 for Process B + 5 for Process A) / 3 = 3.25






*/
