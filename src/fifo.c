#include <stdlib.h>
#include <stdio.h>

#define OK 0
#define FAILURE 1

/**
 * node of an element in in our fifo queue
**/
typedef struct node {
	void* value;  // element stored in node
	struct node* next;  // next node in FIFO queue, appended right after node
} node;

#define NODE_SIZE sizeof(node)

/**
 * Implementation invariants:
 *     len == 1 <--> start == end
 *     end->next == NULL
 *     NOTE that when len == 0, it does not matter the values
 *         of the other parameters since queue is basicly inaccessible,
 *         except for iterate, and unmodifyable in this state.
**/
typedef struct fifo {
	node* start;  // head of queue
	node* end;    // last element of queue, one most recently added
	int len;
} fifo;

fifo* fifoInit() {
	fifo* q = malloc(sizeof(fifo));
	if(q == NULL)
		return q;
	q->len = 0;
	q->start = NULL;
	q->end = NULL;
	return q;
}

/**
 * Helpper function for the prepend and append functions.
 * Inserts elem into q assuming that q is currently empty.
 * Assumes q is not NULL
**/
int addFirst(fifo* q, void* elem) {
	node* e = malloc(NODE_SIZE);
	if(e == NULL)
		return FAILURE;

	e->value = elem;
	e->next = NULL;

	q->start = e;
	q->end = e;
	q->len = 1;
	return OK;
}

int fifoAppend(fifo* q, void* elem) {
	if(q == NULL)
		return FAILURE;
	
	if(q->len == 0)
		return addFirst(q, elem);

	node* newNode = malloc(NODE_SIZE);
	if(newNode == NULL) 
		return FAILURE;

	newNode->value = elem;
	newNode->next = NULL;
	
	q->end->next = newNode;
	q->end = newNode;
	q->len++;
	return OK;
}

int fifoRemove(fifo* q, void** result) {
	if(result == NULL)
		return FAILURE;

	if(q == NULL || q->len == 0) {
		*result = NULL;
		return FAILURE;
	}
	*result = q->start->value;
	// Perform removal
	node* rem = q->start;
	q->start =rem->next;
	free(rem);
	q->len--;

	return OK;
}

int destroyFifo (fifo* q) {
	if(q == NULL || q->len != 0)
		return FAILURE;
	free(q);
	return OK;
}

int getFifoLength(const fifo* queue){
	return queue == NULL ? -1 : queue->len;
}
