#include "queue.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>

typedef struct q_node{
    void *value;
    void *next;
} q_node_t;

typedef struct queue{
    q_node_t *front;
    q_node_t *rear;
    pthread_mutex_t lock;
    pthread_cond_t c;
} queue_t;

queue_t *queue_init() {
    queue_t *queue = malloc(sizeof(queue_t));
    queue->front = NULL;
    queue->rear = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->c, NULL);
    return queue;
}

void queue_enqueue(queue_t *queue, void *value) {
    pthread_mutex_lock(&queue->lock);
    q_node_t *temp = malloc(sizeof(q_node_t));
    temp->value = value;
    temp->next = NULL;
    if (queue->front == NULL) {
        queue->front = temp;
    }
    else {
        queue->rear->next = temp;
    }
    queue->rear = temp;
    pthread_cond_signal(&queue->c);
    pthread_mutex_unlock(&queue->lock);
    return;
}

void *queue_dequeue(queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->front == NULL) { // queue is empty
        pthread_cond_wait(&queue->c, &queue->lock);
    }
    q_node_t *temp = queue->front;
    void *value = temp->value;
    queue->front = temp->next;
    free(temp);
    pthread_mutex_unlock(&queue->lock);
    return value;
}

void queue_free(queue_t *queue) {
    free(queue);
}