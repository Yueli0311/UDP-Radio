#include <stdlib.h>
#include <string.h>
#include "queue.h"

/*
#define ENQ	0
#define DEQ	1
 */
enum {ENQ, DEQ};
// static int last_do = DEQ;

int queue_init(queue_t **q, int size, int capacity)
{
	*q = malloc(sizeof(queue_t));
	if (NULL == *q)
		return -1;
	(*q)->size = size;
	(*q)->capacity = capacity;
	(*q)->front = (*q)->tail = 0;
	(*q)->last_do = DEQ;
	(*q)->base = calloc(capacity, size);
	if (NULL == (*q)->base) {
		free(*q);
		*q = NULL;
		return -1;
	}

	return 0;
}

int queue_empty(const queue_t *q)
{
	return q->front == q->tail && q->last_do == DEQ;
}

int queue_full(const queue_t *q)
{
	return q->front == q->tail && q->last_do == ENQ;
}

int queue_enq(queue_t *q, const void *data)
{
	if (queue_full(q))
		return -1;
	memcpy((char *)q->base + q->tail * q->size, data, q->size);
	q->tail = (q->tail + 1) % q->capacity;

	q->last_do = ENQ;

	return 0;
}

int queue_deq(queue_t *q, void *data)
{
	if (queue_empty(q))
		return -1;
	memcpy(data, (char *)q->base + q->front * q->size, q->size);
	memset((char *)q->base + q->front * q->size, '\0', q->size);
	q->front = (q->front + 1) % q->capacity;

	q->last_do = DEQ;

	return 0;
}

void queue_destroy(queue_t **q)
{
	free((*q)->base);
	(*q)->base = NULL;
	free(*q);
	*q = NULL;
}