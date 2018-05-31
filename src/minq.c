#include <stdlib.h>

#include "minq.h"

int minq_init(queue_t *q, int c) {
	int i;
	
	for (i = 0; i < sizeof(queue_t); i++) {
		*(((char*)q)+i) = 0;
	}
	q->entries = malloc(sizeof(void**)*(c+1));
	if (q->entries == NULL) {
		return 1;	
	}
	q->cap = c+1;
	q->first = 0;
	q->last = 0;
	q->serial = 0;
	return 0;
}

void minq_free(queue_t *q) {
	free(q->entries);
}

int minq_peek(queue_t *q, void **entry) {
	if (q->first == q->last) {
		return -1;
	}
	*entry = *(q->entries+q->first);
	return 0;		
}

int minq_next(queue_t *q, void *entry) {
	if (minq_peek(q, entry)) {
		return -1;
	}
	q->first++;
	q->first %= q->cap;
	return 0;
}

int minq_add(queue_t *q, void *entry) {
	int c;
	c = q->last+1;
	c %= q->cap;
	if (c == q->first) {
		return -1;
	}
	*(q->entries+q->last) = entry;
	q->last = c;
	return ++q->serial;
}
