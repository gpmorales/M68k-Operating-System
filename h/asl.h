#ifndef ASL_H
#define ASL_H

/* semaphore descriptor type */
typedef struct semd_t {
    struct semd_t* s_next;		/* next element on the queue */
    struct semd_t* s_prev;		/* previous element on the queue */
    int* s_semAdd;				/* pointer to the semaphore proper */
    proc_link s_link;			/* pointer/index to the tail of the queue of
                                   processes blocked on this semaphore */
} semd_t;

#endif
