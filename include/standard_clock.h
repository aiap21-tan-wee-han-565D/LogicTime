#ifndef STANDARD_CLOCK_H
#define STANDARD_CLOCK_H

#include "timestamp.h"

/* ---------- Standard Vector Clock Data Structure ---------- */

typedef struct {
    int *v;             // vector clock array
} StandardClockData;

/* ---------- Standard Vector Clock Operations ---------- */

Timestamp standard_create(int n, int pid, ClockType type);
void standard_destroy(Timestamp *ts);
void standard_increment(Timestamp *ts);
void standard_merge(Timestamp *dst, const void *other_data, size_t other_size);
TSOrder standard_compare(const Timestamp *a, const Timestamp *b);
size_t standard_serialize(const Timestamp *ts, void *buffer, size_t bufsize);
void standard_deserialize(Timestamp *ts, const void *buffer, size_t size);
void standard_to_string(const Timestamp *ts, char *buf, size_t bufsize);
Timestamp standard_clone(const Timestamp *ts);

/* ---------- Operations Table ---------- */

extern TimestampOps STANDARD_OPS;

#endif // STANDARD_CLOCK_H