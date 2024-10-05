#include <stdio.h>

#define QUEUE_SIZE 100

struct queue
{
    void *elems[QUEUE_SIZE];
    int r;
    int w;
    int count;
};

void queue_init(struct queue *queue);

int is_empty(struct queue *queue);

void enqueue(struct queue *queue, void *n);

void *dequeue(struct queue *queue);