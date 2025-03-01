/*
    This code is my own work, it was written without consulting code written by other students current or previous or using any AI tools
    George Morales
*/
#include "../h/types.h"
#include "../h/const.h"
#include "../h/procq.e"
#include "../h/asl.e"

semd_t semdTable[MAXPROC];		            /* All semaphore entries are placed in this table */
semd_t* semdFree_h = (semd_t*)ENULL;	    /* List of inactive semaphores */
semd_t* semd_h = (semd_t*)ENULL;	        /* Pointer to the head of the ASL */

/* Local Utility Routines */
void returnSemaphoreToFreeList(semd_t* s);
void removeSemaphoreFromActiveList(semd_t* s);
int addSemaphoreToProcessVector(int* semAddr, proc_t* p);
semd_t* allocateSemaphoreFromFreeList();
void resetSemaphore(semd_t* s);

/*
    Insert the process table entry pointed to by p at the tail of the process queue associated 
    with the semaphore whose address is semAdd. If the semaphore is currently not
    active (there is no descriptor for it in the ASL), allocate a new descriptor from the
    free list, insert it in the ASL (at the appropriate position), and initialize all of the
    fields. If a new semaphore descriptor needs to be allocated and the free list is empty,
    return TRUE. In all other cases return FALSE.
*/
int insertBlocked(int* semAddr, proc_t* p)
{
    // ASL is double linked list where each entry contains a pointer to 
    // a circular queue of processes blocked by the semaphore of that entry
    semd_t* semaphoreDescriptor = getSemaphoreFromActiveList(semAddr);

    // A descriptor is not present for this semAddr and there are no free semaphores available to create a new entry in the ASL for this process.
    if (semdFree_h == (semd_t*)ENULL && semaphoreDescriptor == (semd_t*)ENULL) {
        return TRUE;
    }
    else {
        // An Entry in the ASL is present for this semaphore
        if (semaphoreDescriptor != (semd_t*)ENULL) {
            // Add the process to the tail of the Semaphore's proc queue and the semaphore to the proc's semaphore vector list
            addSemaphoreToProcessVector(semAddr, p);
            proc_link* tp = &semaphoreDescriptor->s_link;
            insertProc(tp, p);
            return FALSE;
        }
        // Otherwise there are inactive/free semaphores with no associated process queues that we can add to the ASL:
        else {
            // Allocate a semaphore descriptor from the free list 
            semd_t* newDescriptor = allocateSemaphoreFromFreeList();

            // Add the process to the tail of the Semaphore's proc queue and the semaphore to the proc's semaphore vector list
            addSemaphoreToProcessVector(semAddr, p);
            proc_link* tp = &newDescriptor->s_link;
            insertProc(tp, p);

            // Add this semaphore to the ASL
            newDescriptor->s_semAdd = semAddr;
            insertSemaphoreIntoActiveList(newDescriptor);
            return FALSE;
        }
    }
}


/*
    Search the ASL for a descriptor of this semaphore. If none is found, return ENULL.
    Otherwise, remove THE FIRST process table entry from the process queue of the appropriate 
    semaphore descriptor and return a pointer to it. If the process queue for this semaphore becomes empty,
    remove the descriptor from the ASL and insert it in the free list of semaphore descriptors.
*/
proc_t* removeBlocked(int* semAddr)
{
    semd_t* semaphoreDescriptor = getSemaphoreFromActiveList(semAddr);

    // No entry is associated with the given address in the ASL
    if (semaphoreDescriptor == (semd_t*)ENULL) {
        return (proc_t*)ENULL;
    }

    // Remove the first proc from the process queue of the ASL semaphore
    proc_link* tp = &semaphoreDescriptor->s_link;
    proc_t* headProc = removeProc(tp);

    // Check if the associated process queue is now empty
    if (tp->next == (proc_t*)ENULL) {
        // Remove the Sem descriptor from the ASL and it the Free List
        removeSemaphoreFromActiveList(semaphoreDescriptor);
        returnSemaphoreToFreeList(semaphoreDescriptor);
    }

    // Remove this semaphore from the process's semvac vector
    removeSemaphoreFromProcessVector(semAddr, headProc);
    return headProc;
}


/*
    Remove the process table entry pointed to by p from the queues associated with the
    appropriate semaphores on the ASL. If the desired entry does not appear in any of
    the process queues (an error condition), return ENULL. Otherwise, return p.
*/
proc_t* outBlocked(proc_t* p)
{
    // Iterate through Active Semaphores List (ASL) process queues
    semd_t* semaphoreDescriptor = semd_h;
    int processRemoved = FALSE;

    while (semaphoreDescriptor != (semd_t*)ENULL) {
        // Attempt to remove the given process from this ASL entry's proc queue
        proc_link* tp = &semaphoreDescriptor->s_link;
        int* semAddr = semaphoreDescriptor->s_semAdd;

        // The process queue is empty
        if (tp->next == (proc_t*)ENULL) {
            semaphoreDescriptor = semaphoreDescriptor->s_next; 
            continue;
        }

        int removalResult = outProc(tp, p) == (proc_t*)ENULL ? 0 : 1;

        // Remove this semaphore from the process's semvac vector
        if (removalResult) {
            // For each semaphore that was blocked on
            removeSemaphoreFromProcessVector(semAddr, p);
            processRemoved = TRUE;
        }

        // If this Active Semaphore's process queue becomes empty, remove it from the ASL
        if (removalResult && tp->next == (proc_t*)ENULL) {
            semd_t* nextSemaphoreDescriptor = semaphoreDescriptor->s_next; 
            removeSemaphoreFromActiveList(semaphoreDescriptor);
			returnSemaphoreToFreeList(semaphoreDescriptor);
            semaphoreDescriptor = nextSemaphoreDescriptor;
            continue;
        }

        semaphoreDescriptor = semaphoreDescriptor->s_next;
    }

    // If the process did not appear in any process queue, return ENULL
    return processRemoved == TRUE ? p : (proc_t*)ENULL;
}


/*
    Return a pointer to the process table entry that is at the head of the process queue associated
    with semaphore semAdd. If the list is empty, return ENULL.
*/
proc_t* headBlocked(int* semAddr)
{
    semd_t* semaphoreDescriptor = getSemaphoreFromActiveList(semAddr);

    // There is no semaphore descriptor associated with this address
    if (semaphoreDescriptor == (semd_t*)ENULL) {
        return (proc_t*)ENULL;
    }

    return headQueue(semaphoreDescriptor->s_link);
}


/*
    Initialize the semaphore descriptor free list.

*/
void initSemd()
{
    // Use the first element of semdTable as free list head
    semdFree_h = &semdTable[0];
    semdFree_h->s_prev = (semd_t*)ENULL;

    semd_t* prevSemd = semdFree_h;;
    int i;
    for (i = 1; i < MAXPROC; i++) {
        semd_t* currSemd = &semdTable[i];
        currSemd->s_prev = prevSemd;
        prevSemd->s_next = currSemd;
        prevSemd = currSemd;
    }

    // Null terminate the free list
    prevSemd->s_next = (semd_t*)ENULL;
}


/*
    This function will be used to determine if there are any semaphores on the ASL.
    Return FALSE if the ASL is empty or TRUE if not empty.
*/
int headASL()
{
    return semd_h != (semd_t*)ENULL;
}


/*
    Return a Semaphore Descriptor to the free list, making it available for future use 
    to manage blocked processes.
*/
void returnSemaphoreToFreeList(semd_t* s)
{
    if (s == (semd_t*)ENULL) {
        return;
    }
    resetSemaphore(s);

    //  When the free list is empty, let s be the first element
    if (semdFree_h == (semd_t*)ENULL) {
        semdFree_h = s;
        return;
    }

    // Insert at the head of the free list
    s->s_next = semdFree_h;
    semdFree_h->s_prev = s;
    semdFree_h = s;
}


/*
    Acquire a semaphore descriptor from the free list and initialize an associated 
    process queue for managing blocked processes.
*/
semd_t* allocateSemaphoreFromFreeList()
{
    semd_t* semaphoreDescriptor = semdFree_h;

    // No Inactive Semaphores
    if (semaphoreDescriptor == (semd_t*)ENULL) {
        return (semd_t*)ENULL;
    }

    // Remove and return the head of the list
    semdFree_h = semaphoreDescriptor->s_next;
    resetSemaphore(semaphoreDescriptor);
    return semaphoreDescriptor;
}


/*
    Insert a new Semaphore Descriptor into the ASL, which is ordered by semaphore addresses.
    Uses insertion sort starting from the head.
*/
void insertSemaphoreIntoActiveList(semd_t* s)
{
    // Insertion into empty ASL
    if (semd_h == (semd_t*)ENULL) {
        semd_h = s;
        s->s_next = (semd_t*)ENULL;
        s->s_prev = (semd_t*)ENULL;
        return;
    }

    semd_t* currSemd = semd_h;
    semd_t* insertSemd = s;

    // Insertion required at head (direct pointer comparison)
    if (insertSemd->s_semAdd < currSemd->s_semAdd) {
        insertSemd->s_next = currSemd;
        insertSemd->s_prev = (semd_t*)ENULL;
        currSemd->s_prev = insertSemd;
        semd_h = insertSemd;
        return;
    }

    // Otherwise insertion will happen in the middle or at end of the ASL
    semd_t* prevSemd;
    while (currSemd != (semd_t*)ENULL && insertSemd->s_semAdd > currSemd->s_semAdd) {
        prevSemd = currSemd;
        currSemd = currSemd->s_next;
    }

    // Insert this entry before curr and after prev semd_t
    prevSemd->s_next = insertSemd;
    insertSemd->s_prev = prevSemd;
    insertSemd->s_next = currSemd;
    if (currSemd != (semd_t*)ENULL) {
        currSemd->s_prev = insertSemd;
    }
}


/*
    Remove the Semaphore Descriptor associated with the given address from the ASL.
*/
void removeSemaphoreFromActiveList(semd_t* s)
{
    if (semd_h == (semd_t*)ENULL || s == (semd_t*)ENULL) {
        return;
    }

    semd_t* prevSemd = s->s_prev;
    semd_t* nextSemd = s->s_next;

    // ASL with single element
    if (semd_h == s && nextSemd == (semd_t*)ENULL && prevSemd == (semd_t*)ENULL) {
        semd_h = (semd_t*)ENULL;
        resetSemaphore(s);
        return;
    }

    // Removal at the head of the ASL
    if (s == semd_h) {
        semd_h = nextSemd;
        nextSemd->s_prev = (semd_t*)ENULL;
        resetSemaphore(s);
        return;
    }

    // Remove the Sem descriptor from multi-element ASL
    prevSemd->s_next = nextSemd;
    if (nextSemd != (semd_t*)ENULL) {
        nextSemd->s_prev = prevSemd;
    }
    resetSemaphore(s);
}


/*
    Retrieve the Semaphore Descriptor (semd_t) associated with the given semaphore address (semAddr)
    from the ASL. Returns ENULL if the descriptor is not found.
*/
semd_t* getSemaphoreFromActiveList(int* semAddr) 
{
    semd_t* semDescriptor = semd_h;

    while (semDescriptor != (semd_t*)ENULL) {
        if (semDescriptor->s_semAdd == semAddr) {
            return semDescriptor;
        }
        semDescriptor = semDescriptor->s_next;
    }

    return (semd_t*)ENULL;
}


/*
    Add the semaphore specified by semAddr to the vector of active semaphores 
    associated with the given process.
*/
int addSemaphoreToProcessVector(int* semAddr, proc_t* p)
{
    if (p == (proc_t*)ENULL || semAddr == (int*)ENULL) {
        return FALSE;
    }

    // Array of pointers to active semaphores blocking this process
    int** activeSemaphoreAddresses = p->semvec;

    int i;
    for (i = 0; i < SEMMAX; i++) {
        int* activeSemAddr = activeSemaphoreAddresses[i];
        if (activeSemAddr == (int*)ENULL) {
            activeSemaphoreAddresses[i] = semAddr;
            return TRUE;
        }
    }

    return FALSE;
}


/*
    Remove the semaphore specified by semAddr from the vector of active semaphores 
    associated with the given process.
*/
int removeSemaphoreFromProcessVector(int* semAddr, proc_t* p)
{
    if (p == (proc_t*)ENULL || semAddr == (int*)ENULL) {
        return FALSE;
    }

    // Array of pointers to active semaphores blocking this process
    int** activeSemaphoreAddresses = p->semvec;

    int i;
    for (i = 0; i < SEMMAX; i++) {
        int* processActiveSemAddr = activeSemaphoreAddresses[i];
        if (semAddr == processActiveSemAddr) {
            activeSemaphoreAddresses[i] = (int*)ENULL;
            return TRUE;
        }
    }

    return FALSE;
}


/*
    Reset the fields of the given semaphore descriptor
*/
void resetSemaphore(semd_t* s) 
{
    s->s_next = (semd_t*)ENULL;
    s->s_prev = (semd_t*)ENULL;
    s->s_semAdd = (int*)ENULL;
    s->s_link.index = ENULL;
    s->s_link.next = (proc_t*)ENULL;
}
