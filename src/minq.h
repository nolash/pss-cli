#ifndef MINQ_H_
#define MINQ_H_

#include <stdlib.h>
#include <stdint.h>

typedef struct queue_t_ {
	uint32_t cap;
	uint32_t serial;
	uint32_t first;
	uint32_t last;
	void **entries;
} queue_t;

/***
 * \brief initialize a new queue
 *
 * \param q queue object to initialize
 * \param c max capacity of queue
 * \return 0 on success, 1 if queue resources couldn't be accessed
 */
int minq_init(queue_t *q, int c);

/***
 * \brief free a queue
 * 
 * \param q queue object to free
 */
void minq_free(queue_t *q);

/***
 * \brief peek at next element in queue
 *
 * \param q queue object
 * \param entry output pointer to entry object
 * \return 0 on success, -1 if no data is available
 */
int minq_peek(queue_t *q, void **entry);
	
/**
 * \brief advance to next element in queue
 *
 * puts the entry currently pointed to in the entry pointer, and advances to the next entry
 * 
 * \param q queue object
 * \param entry output pointer to entry object
 * \return 0 on success, -1 if no data is available
 */
int minq_next(queue_t *q, void *entry);
	
/***
 * \brief appends a new item to the end of the queue
 *
 * \param q queue object
 * \param entry entry to add
 * \return serial number of added entry on success, -1 if queue is full.
 */
int minq_add(queue_t *q, void *entry);

#endif // MINQ_H_
