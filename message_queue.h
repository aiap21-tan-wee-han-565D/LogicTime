#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <pthread.h>
#include "timestamp.h"

/* ---------- Message Structure ---------- */

typedef struct Message {
    int from;
    int to;
    void *timestamp_data;   // serialized timestamp data
    size_t timestamp_size;  // size of timestamp data
    ClockType clock_type;   // type of clock used
    char payload[64];
    struct Message *next;
} Message;

/* ---------- Thread-Safe Message Queue ---------- */

typedef struct {
    Message *head, *tail;
    int size;
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
} MsgQueue;

/* ---------- Message Queue Operations ---------- */

void mq_init(MsgQueue *q);
void mq_destroy(MsgQueue *q);
void mq_push(MsgQueue *q, Message *m);
Message* mq_try_pop(MsgQueue *q);  // Non-blocking pop; returns NULL if empty

#endif // MESSAGE_QUEUE_H