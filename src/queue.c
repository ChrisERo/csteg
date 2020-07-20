/**
 * Christian Edward Rodriguez (cer95)
 * CS 4411 Project 0
 * 02/01/2018
**/

#include <stdlib.h>
#include <stdio.h>

#define OK 0
#define FAILURE -1
#define NODE_SIZE sizeof(node_t)

/**
 * node of an element in in queue_t
 * value is the element stored in node
 * next points to the next element's node in queue
**/
typedef struct node_t {
	void* value;
	struct node_t* next;
} node_t;

/**
 * This structure implements queue_t as described in the header file 
 * start represents the address of the first element's node,
 * end to represent that of the last element's node.
 * len is the number of elements in the queue
 *
 * Implementation invariants:
 * when len == 1, start == end
 * end->next == NULL
 * NOTE that when len == 0, it does not matter the values
 *  of the other parameters since queue is basicly inaccessible,
 *  except for iterate, and unmodifyable in this state.
**/
typedef struct queue_t {
	node_t* start;
	node_t* end;
	int len;
} queue_t;

queue_t* queue_new() { //DEALOC both queue and addresses when destroy
	queue_t* q = malloc(sizeof(queue_t));
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
**/
int addFirst(queue_t* q, void* elem) {
	node_t* e = malloc(NODE_SIZE);
	if(q== NULL || e == NULL)
		return FAILURE;

	e->value = elem;
	e->next = NULL;
	q->start = e;
	q->end = e;
	q->len = 1;
	return OK;
}

int queue_prepend(queue_t* q, void* elem) {
	if(q == NULL)
		return FAILURE;

	if(q->len == 0)
		return addFirst(q, elem);

	node_t* newNode = malloc(NODE_SIZE);
	if(newNode == NULL) 
		return FAILURE;

	newNode->value = elem;
	newNode->next = q->start;
	q->start = newNode;
	q->len++;
	return OK;
}

int queue_append(queue_t* q, void* elem) {
	if(q == NULL)
		return FAILURE;
	
	if(q->len == 0)
		return addFirst(q, elem);

	node_t* newNode = malloc(NODE_SIZE);
	if(newNode == NULL) 
		return FAILURE;

	newNode->value = elem;
	newNode->next = NULL;
	q->end->next = newNode;
	q->end = newNode;
	q->len++;
	return OK;
}

/**
 * Helpper for dequeue and delete functions,
 * Removes the first element of q, the one at start,
 * while decrementing q->len
 * Assumes that q points to a valid queue_t structure
**/
void removeFirst(queue_t* q) {
	node_t* rem = q->start;
	q->start =rem->next;
	free(rem);
	q->len--;
}

int queue_dequeue(queue_t* q, void** result) {
	if(result == NULL)
		return FAILURE;

	if(q == NULL || q->len == 0) {
		*result = NULL;
		return FAILURE;
	}
	*result = q->start->value;
	removeFirst(q);
	return OK;
}

typedef void (*func_t)(void*, void*);
int queue_iterate(queue_t* q, func_t f, void* t) {
	if(q == NULL || f == NULL)
		return FAILURE;
	node_t* current = q->start;
	for(int i = 0; i < q->len; i++) {
		f(current->value, t);
		current = current->next;
	}
	return OK;
}

int queue_free (queue_t* q) {
	if(q == NULL || q->len != 0)
		return FAILURE;
	free(q);
	return OK;
}


int queue_length(const queue_t* queue){
	return queue == NULL ? FAILURE : queue->len;
}

//NOTE: This implementation returns FAILIURE when nothing is deleted
int queue_delete(queue_t* queue, void* item) {
	if(queue == NULL || queue->len == 0)
		return FAILURE;

	node_t* current = queue->start;
	if(current->value == item) {
		removeFirst(queue);
		return OK;
	}
	for(int i = 1; i < queue->len; i++, current = current->next) {
		if(current->next->value == item) {
			if(i  == queue->len - 1) { //Going to delete last element
				queue->end = current;
			}
			node_t* rem = current->next;
			current->next = rem->next;
			free(rem);
			queue->len--;
			return OK;
		}
	}
	return FAILURE;
}
