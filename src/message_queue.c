#include <stdlib.h>
#include <pthread.h>
#include "message_queue.h"

/* ---------- Thread-Safe Message Queue Implementation ---------- */

void mq_init(MsgQueue *q) {
    q->head = q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cv, NULL);
}

void mq_destroy(MsgQueue *q) {
    pthread_mutex_lock(&q->mtx);
    Message *cur = q->head;
    while (cur) {
        Message *nxt = cur->next;
        if (cur->timestamp_data) {
            free(cur->timestamp_data);
        }
        free(cur);
        cur = nxt;
    }
    q->head = q->tail = NULL;
    q->size = 0;
    pthread_mutex_unlock(&q->mtx);
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->cv);
}

void mq_push(MsgQueue *q, Message *m) {
    m->next = NULL;
    pthread_mutex_lock(&q->mtx);
    if (!q->tail) q->head = q->tail = m;
    else { q->tail->next = m; q->tail = m; }
    q->size++;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mtx);
}

// Non-blocking pop; returns NULL if empty
Message* mq_try_pop(MsgQueue *q) {
    pthread_mutex_lock(&q->mtx);
    Message *m = q->head;
    if (m) {
        q->head = m->next;
        if (!q->head) q->tail = NULL;
        q->size--;
    }
    pthread_mutex_unlock(&q->mtx);
    return m;
}