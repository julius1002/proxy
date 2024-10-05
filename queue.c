#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void queue_init(struct queue *queue)
{
    queue->r = 0;
    queue->w = 0;
    queue->count = 0;
}

int is_empty(struct queue *queue)
{
    return queue->count == 0;
}

void enqueue(struct queue *queue, void *n)
{
    if (queue->count == QUEUE_SIZE)
    {
        // fprintf(stderr, "Queue is full! Cannot enqueue %d\n", n);
        exit(1);
    }

    queue->elems[queue->w] = n;
    queue->w = (queue->w + 1) % QUEUE_SIZE;
    queue->count++;
}

void *dequeue(struct queue *queue)
{
    if (is_empty(queue))
    {
        printf("Queue is empty! Cannot dequeue\n");
        return NULL;
    }

    void *n = queue->elems[queue->r];
    queue->r = (queue->r + 1) % QUEUE_SIZE;
    queue->count--;
    return n;
}