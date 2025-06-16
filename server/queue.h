#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct {
	void *base;
	int size;
	int capacity;
	int front;
	int tail;
	int last_do;
}queue_t;

extern int queue_init(queue_t **q, int size, int capacity);

extern int queue_empty(const queue_t *q);

extern int queue_full(const queue_t *q);

extern int queue_enq(queue_t *q, const void *data);

extern int queue_deq(queue_t *q, void *data);

extern void queue_destroy(queue_t **q);

#endif
