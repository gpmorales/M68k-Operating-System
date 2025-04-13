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
void insertSemaphoreIntoActiveList(semd_t* s);
void addSemaphoreToProcessVector(int* semAddr, proc_t* p);
semd_t* allocateSemaphoreFromFreeList();
semd_t* getSemaphoreFromActiveList(int* semAddr);
void resetSemaphore(semd_t* s);
void removeSemaphoreFromProcessVector(int* semAddr, proc_t* p);


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
    if (semaphoreDescriptor == (semd_t*)ENULL && semdFree_h == (semd_t*)ENULL) {
        return TRUE;
    }
    else {
        // An Entry in the ASL is present for this semaphore
        if (semaphoreDescriptor != (semd_t*)ENULL) {
            // Add the process to the tail of the Semaphore's proc queue and the semaphore to the proc's semaphore vector list
            addSemaphoreToProcessVector(semAddr, p);
            insertProc(&semaphoreDescriptor->s_link, p);
            return FALSE;
        }
        // Otherwise there are inactive/free semaphores with no associated process queues that we can add to the ASL:
        else {
            // Allocate a semaphore descriptor from the free list 
            semd_t* newDescriptor = allocateSemaphoreFromFreeList();
            newDescriptor->s_semAdd = semAddr;

            // Add the process to the tail of the Semaphore's proc queue and the semaphore to the proc's semaphore vector list
            addSemaphoreToProcessVector(semAddr, p);
            insertProc(&newDescriptor->s_link, p);

            // Add this semaphore to the ASL
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
    proc_t* process = removeProc(&semaphoreDescriptor->s_link);

    // Check if the associated process queue is now empty
    if (semaphoreDescriptor->s_link.next == (proc_t*)ENULL) {
        // Remove the Sem descriptor from the ASL and put it on the Free List
        removeSemaphoreFromActiveList(semaphoreDescriptor);
    }

    // Remove this semaphore from the process's semvac vector
    removeSemaphoreFromProcessVector(semAddr, process);
    return process;
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
    int processRemovedAtLeastOnce = FALSE;

    while (semaphoreDescriptor != (semd_t*)ENULL) {
        // Attempt to remove the given process from this ASL entry's proc queue
        proc_link* tp = &semaphoreDescriptor->s_link;
        int* semAddr = semaphoreDescriptor->s_semAdd;
        semd_t* nextSemaphoreDescriptor = semaphoreDescriptor->s_next; 

        int wasRemoved = outProc(tp, p) != (proc_t*)ENULL ? 1 : 0;

        // Remove this semaphore from the process's semvac vector
        if (wasRemoved) {
            // We found and removed p from this semaphores's queue
            *semAddr = *semAddr + 1;
            processRemovedAtLeastOnce = TRUE;

            // Remove semAddr from the process's semvec[]
            removeSemaphoreFromProcessVector(semAddr, p);

            // If this Active Semaphore's process queue becomes empty, remove it from the ASL and put the semd back on the free list
            if (tp->next == (proc_t*)ENULL) {
                removeSemaphoreFromActiveList(semaphoreDescriptor);
            }
        }

        semaphoreDescriptor = nextSemaphoreDescriptor;
    }

    // If the process did not appear in any process queue, return ENULL
    return processRemovedAtLeastOnce == TRUE ? p : (proc_t*)ENULL;
}


/*
    Return a pointer to the process table entry that is at the head of the process queue associated
    with semaphore semAdd. If the list is empty, return ENULL.
*/
proc_t* headBlocked(int* semAddr)
{
    semd_t* semaphoreDescriptor = getSemaphoreFromActiveList(semAddr);

    if (semaphoreDescriptor != (semd_t*)ENULL && semaphoreDescriptor->s_link.next != (proc_t*)ENULL) {
        return headQueue(semaphoreDescriptor->s_link);
    }

    // There is no semaphore descriptor associated with this address or list is empty
    return (proc_t*)ENULL;
}


/*
    Initialize the semaphore descriptor free list.

*/
void initSemd()
{
    // Use the first element of semdTable as free list head
    semdFree_h = &semdTable[0];
    semdTable[0].s_prev = (semd_t*)ENULL;
    semdTable[0].s_next = &semdTable[1];

    int i;
    for (i = 1; i < MAXPROC - 1; i++) {
        semdTable[i].s_next = &semdTable[i + 1];
        semdTable[i].s_prev = &semdTable[i - 1];
    }

    semdTable[MAXPROC - 1].s_next = (semd_t*)ENULL;
    semdTable[MAXPROC - 1].s_prev = &semdTable[MAXPROC - 2];

    // Null terminate the free list
    semd_h = (semd_t*)ENULL;
}


/*
    This function will be used to determine if there are any semaphores on the ASL.
    Return FALSE if the ASL is empty or TRUE if not empty.
*/
int headASL()
{
    return (semd_t*)ENULL != (semd_h);
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
    semd_t* prevSemd = currSemd;
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
    Remove the Semaphore Descriptor associated with the given address from the ASL and returns it to the Free List.
*/
void removeSemaphoreFromActiveList(semd_t* s)
{
    // Edge case
    if (semd_h == (semd_t*)ENULL || s == (semd_t*)ENULL) {
        return;
    }

    // Removal of head
    if (s == semd_h) {
        semd_h = s->s_next;
        if (semd_h != (semd_t*)ENULL) {
            semd_h->s_prev = (semd_t*)ENULL;
        }
    }
    // After head
    else {
        if (s->s_next != (semd_t*)ENULL) {
            s->s_next->s_prev = s->s_prev;
        }
        if (s->s_prev != (semd_t*)ENULL) {
            s->s_prev->s_next = s->s_next;
        }
    }

    returnSemaphoreToFreeList(s);
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
void addSemaphoreToProcessVector(int* semAddr, proc_t* p)
{
    if (p == (proc_t*)ENULL || semAddr == (int*)ENULL) {
        return;
    }

    int i;
    for (i = 0; i < SEMMAX; i++) {
        if (p->semvec[i] == (int*)ENULL) {
            p->semvec[i] = semAddr;
            return;
        }
    }
}


/*
    Remove the semaphore specified by semAddr from the vector of active semaphores 
    associated with the given process.
*/
void removeSemaphoreFromProcessVector(int* semAddr, proc_t* p)
{
    if (p == (proc_t*)ENULL || semAddr == (int*)ENULL) {
        return;
    }

    int i;
    for (i = 0; i < SEMMAX; i++) {
        if (p->semvec[i] == semAddr) {
            p->semvec[i] = (int*)ENULL;
            return;
        }
    }
}


/*
    Return a Semaphore Descriptor to the free list, making it available for future use to manage blocked processes.
*/
void returnSemaphoreToFreeList(semd_t* s)
{
    if (s == (semd_t*)ENULL) {
        return;
    }

    resetSemaphore(s);

    //  When the free list is empty, let s be the first element
    s->s_next = semdFree_h;
    semdFree_h = s;
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
