/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../h/const.h"
#include "../h/types.h"
#include "../h/procq.e"

#define FREE_LIST 0
proc_t procTable[MAXPROC];		            /* Universal table of all processes */
proc_t* procFree_h = (proc_t*)ENULL;		/* List which contains all unused proc_t in the procTable */

char msgbuf[128];	            /* nonrecoverable error message before shut down */

/* Local Utility Routines */
void panic(char* message);
int findAvailableQueueSlot(proc_t* p);
void resetProcess(proc_t* p);


/*
    Insert the element pointed to by p into the process queue where tp contains the
    pointer/index to the tail (last element). Update the tail pointer accordingly. 
    If the process is already in the SEMMAX queues, call the panic function.
*/
void insertProc(proc_link* tp, proc_t* p)
{
    // Ensure the process is in less than SEMMAX queues
    if (p->qcount >= SEMMAX) {
        panic("proc_t* p is on the maximum number of queues.");
    }
    else {
        // Handle insertion when process queue is empty
        if (tp->next == (proc_t*)ENULL) {
            tp->next = p;
            tp->index = findAvailableQueueSlot(p);
            p->p_link[tp->index].index = tp->index;		// the process is the tail & head
            p->p_link[tp->index].next = p;
            p->qcount++;
        }
        else {
            // Find a free proc link index for this process
            int proc_queue_idx = findAvailableQueueSlot(p);
            int tail_queue_idx = tp->index;

            // Initialize the new process's link
            proc_t* headProc = tp->next->p_link[tail_queue_idx].next;
            int head_queue_idx = tp->next->p_link[tail_queue_idx].index;

            // Tail points to new Tail
            tp->next->p_link[tail_queue_idx].next = p;			    	// old tail points to new tail (p)
            tp->next->p_link[tail_queue_idx].index = proc_queue_idx;	// the old tail's next index is the idx we found for this new proc

            // New Tail points to Head
            p->p_link[proc_queue_idx].next = headProc;				    // new tail (p) points to the head
            p->p_link[proc_queue_idx].index = head_queue_idx;		    // the index of the queue where the next proc_t is located 

            // Update tail pointer and the new process's fields
            p->qcount++;
            tp->next = p;                   // new tail
            tp->index = proc_queue_idx;     // update the index for which the queue tail belongs to
        }
    }
}


/*
    Remove the first element from the process queue whose tail is pointed to by tp.
    Return ENULL if the queue was initially empty, otherwise return the pointer to the removed
    element. Update the pointer to the tail of the queue if the necessary.
*/
proc_t* removeProc(proc_link* tp)
{
    // Handle empty process queue
    if (tp->next == (proc_t*)ENULL) {
        return (proc_t*)ENULL;
    }

    // Fetch the head (tp->next->link[idx].next) and the tail (tp->next)
    proc_t* headProc = headQueue(*tp);
    proc_t* tailProc = tp->next;
    int tail_queue_idx = tp->index;

    // For single element process queue
    if (headProc == tailProc) {
        tailProc->p_link[tail_queue_idx].next = (proc_t*)ENULL;
        tailProc->p_link[tail_queue_idx].index = ENULL;
        tailProc->qcount--;
        tp->next = (proc_t*)ENULL;
        tp->index = ENULL;
        return tailProc;
    }

    // Grab the index of the heads next element's corresponding queue
    int head_queue_idx = tailProc->p_link[tail_queue_idx].index;

    // Update the tail next to be the heads next proc (using the index at tp->next)
    // and the index to that of the index from the head link (this is the index for head->next)
    tailProc->p_link[tail_queue_idx].index = headProc->p_link[head_queue_idx].index;
    tailProc->p_link[tail_queue_idx].next = headProc->p_link[head_queue_idx].next;

    // Remove the process's proc_link from this queue and update fields
    headProc->p_link[tail_queue_idx].index = ENULL;
    headProc->p_link[tail_queue_idx].next = (proc_t*)ENULL;
    headProc->qcount--;
    return headProc;
}


/*
    Remove the process table entry pointed to by p from the queue whose tail is pointed to by tp.
    Update the pointer to the tail of the queue if necessary. If the desired entry is not the
    in the defined queue (an error condition), return ENULL. Otherwise, return p.
*/
proc_t* outProc(proc_link* tp, proc_t* p)
{
    // Handle empty process queue
    if (tp == (proc_link*)ENULL || tp->next == (proc_t*)ENULL) {
        return (proc_t*)ENULL;
    }

    // Fetch the head (tp->next->link[tp->index].next) and the tail (tp->next)
    proc_t* headProc = headQueue(*tp);
    proc_t* tailProc = tp->next;

    // For single element process queue
    if (headProc == tailProc) {
        if (tp->next == p) {
            return removeProc(tp);
        } 
        else {
            return (proc_t*)ENULL;
        }
    }

    // Case where removal of the first element in the queue is required
    if (p == headProc) {
        return removeProc(tp);
    }

    // Search queue for given process
    int prev_queue_idx = tp->index;								    // tail index
    int curr_queue_idx = tailProc->p_link[tp->index].index;         // head index
    proc_t* prevProc;
    proc_t* currProc = headProc;

    while (currProc != p && currProc != tailProc) {
        // Keep track of prev process
        prevProc = currProc;
        // Update the prev process index
        prev_queue_idx = curr_queue_idx;
        // The next index will be from the curr process's next index
        curr_queue_idx = currProc->p_link[curr_queue_idx].index;
        // Recall that tail_queue_idx is the prev procs index
        currProc = currProc->p_link[prev_queue_idx].next;
    }

    // We reached the tail and it is not the target
    if (currProc == tailProc && p != tailProc) {
        return (proc_t*)ENULL;
    }
    // Update the tail pointer if the target is the tail
    else if (currProc == tailProc && p == tailProc) {
        tp->next = prevProc;  // proc before tail becomes new tail
        tp->index = prev_queue_idx; 
    }

    // Remove the current process from this queue by pointing the prev process next and index to the next process
    prevProc->p_link[prev_queue_idx].index = p->p_link[curr_queue_idx].index;
    prevProc->p_link[prev_queue_idx].next = p->p_link[curr_queue_idx].next;

    // Update the removed process's fields
    p->p_link[curr_queue_idx].next = (proc_t*)ENULL;
    p->p_link[curr_queue_idx].index = ENULL;
    p->qcount--;

    return p;
}


/*
    Return ENULL if the procFree list is empty.
    Otherwise, remove an element from the procFree list and return a pointer to it.
*/
proc_t* allocProc() 
{
    // Free Process List is empty
    if (procFree_h == (proc_t*)ENULL) {
        return (proc_t*)ENULL;
    }

    // First element of the procFree list
    proc_t* allocatedProc = procFree_h;

    // Free List only has one element
    if (allocatedProc->p_link[FREE_LIST].next == (proc_t*)ENULL) {
        procFree_h = (proc_t*)ENULL;
        return allocatedProc;
    }

    // Remove the first element of the Free Process List and update pointers
    procFree_h = allocatedProc->p_link[FREE_LIST].next;
    allocatedProc->p_link[FREE_LIST].next = (proc_t*)ENULL;
    allocatedProc->p_link[FREE_LIST].index= ENULL;
    return allocatedProc;
}


/*
    Reinsert the element pointed to by p into the procFree list.
*/
void freeProc(proc_t* p)
{
    resetProcess(p);

    // procFree list is empty
    if (procFree_h == (proc_t*)ENULL) {
        procFree_h = p;
        return;
    }

    // Insert the element into the procFree list
    proc_t* proc = procFree_h;
    while (proc->p_link[FREE_LIST].next != (proc_t*)ENULL) {
        proc = proc->p_link[FREE_LIST].next;
    }

    proc->p_link[FREE_LIST].next = p;
}


/*
    Return a pointer to the process table entry at the head of the queue. The tail of the
    queue of the queue is pointed to by tp.
*/
proc_t* headQueue(proc_link tp)
{
    // Check if queue is empty
    if (tp.next == (proc_t*)ENULL) {
        return (proc_t*)ENULL;
    }
    // tp->next is the tail proc_t & tp->next->p_link[idx] is the head proc_t
    return tp.next->p_link[tp.index].next;
}


/*
    Initialize the procFree List to contain all the elements of the array procTable.
    Will be called only once during data structure initialization
*/
void initProc() 
{
    // The free list contains `proc_t` entries that are unused and available to be allocated for handling and managing new processes. 
    // It abstracts the process lifecycle by allowing dynamic allocation and reuse.
    procFree_h = &procTable[0];

    // Traverse procTable
    int i;
    for (i = 0; i < MAXPROC; i++) 
    {
        // Set the current nodes next to the next process in the table
		// Remove all processor states
        resetProcess(&procTable[i]);
		procTable[i].mm_trap_old_state = (state_t*)ENULL;
		procTable[i].mm_trap_new_state = (state_t*)ENULL;
		procTable[i].sys_trap_old_state = (state_t*)ENULL;
		procTable[i].sys_trap_new_state = (state_t*)ENULL;
		procTable[i].prog_trap_old_state = (state_t*)ENULL;
		procTable[i].prog_trap_new_state = (state_t*)ENULL;
        if (i != 19) {
			procTable[i].p_link[FREE_LIST].next = &procTable[i + 1];
        }
    }
}


/*
    Find an available queue slot (p_link index) for the given process that it is
    not already part of. Returns the index of the free slot, or ENULL if the
    process is already on the maximum allowable number of semaphore queues.
*/
int findAvailableQueueSlot(proc_t* p)
{
    int i;
    for (i = 0; i < SEMMAX; i++) {
        if (p->p_link[i].next == (proc_t*)ENULL) {
            return i;
        }
    }
    return ENULL;
}


/*
    Reset the given proc_t's fields and remove it from all associated process queues.
*/
void resetProcess(proc_t* p)
{
    // Process does not belong to any queues
    p->qcount = 0;

	// The of processor time used by this process is 0
    p->total_processor_time = 0;
    p->last_start_time = 0;

    // Remove any semaphores or proc links associated with this proc_t entry
    int i;
    for (i = 0; i < SEMMAX; i++) {
        p->p_link[i].next = (proc_t*)ENULL;
        p->p_link[i].index = ENULL;
        p->semvec[i] = (int*)ENULL;
    }

    // Remove all progeny links
	p->parent_proc = (proc_t*)ENULL;
	p->sibling_proc = (proc_t*)ENULL;
	p->children_proc = (proc_t*)ENULL;
}


// Panic function
void panic(char* message) 
{
    register char *i = msgbuf;
    while ((*i++ = *message++) != '\0')
        ;
         asm("	trap	#0");
}
