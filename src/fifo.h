#ifndef __FIFO__
#define __FIFO__

/*
 * fifo is a pointer to a FIFO queue, one in which elements are ordered based on
 * when they were added to the queue and the first item added to the queue is
 * the first to be removed.
 * 
 * This queue is very basic; it only implements the core functionality of a
 * FIFO queue (appending and popping as fifoAppend and fifoRemove respectively),
 * all that is required for this project
 */
typedef struct fifo fifo;

/*
 * Return an empty queue on success and NULL on failiure
 */
fifo* fifoInit();

/*
 * Frees all memory allocated to queue if queue_t is empty and returns 0. If
 * queue is NULL or is not empty, returns 1
 */
int destroyFifo(fifo*);

/*
 * Appends a void* to a queue (both specified as parameters). If append executed
 * successfully, return 0, else return 1
 */
int fifoAppend(fifo*, void*);

/*
 * Remove and store first element added in queue to void* memory location.
 * Return 0 if dequeue and storage executed successfully and 1 otherwise
 */
int fifoRemove(fifo*, void**);

/*
 * Return the number of items in the queue, or -1 if queue is NULL
 */
int getFifoLength(const fifo* queue);

#endif /*__QUEUE_H__*/
