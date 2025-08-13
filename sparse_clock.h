#ifndef SPARSE_CLOCK_H
#define SPARSE_CLOCK_H

#include "timestamp.h"

/* ---------- Sparse Vector Clock Data Structures ---------- */

// Sparse vector clock entry
typedef struct {
    int pid;
    int counter;
} SparseEntry;

// Sparse vector clock data
typedef struct {
    SparseEntry *entries;
    int count;          // number of non-zero entries
    int capacity;       // allocated capacity
} SparseClockData;

/* ---------- Sparse Vector Clock Operations ---------- */

Timestamp sparse_create(int n, int pid, ClockType type);
void sparse_destroy(Timestamp *ts);
void sparse_increment(Timestamp *ts);
void sparse_merge(Timestamp *dst, const void *other_data, size_t other_size);
TSOrder sparse_compare(const Timestamp *a, const Timestamp *b);
size_t sparse_serialize(const Timestamp *ts, void *buffer, size_t bufsize);
void sparse_deserialize(Timestamp *ts, const void *buffer, size_t size);
void sparse_to_string(const Timestamp *ts, char *buf, size_t bufsize);
Timestamp sparse_clone(const Timestamp *ts);

/* ---------- Operations Table ---------- */

extern TimestampOps SPARSE_OPS;

#endif // SPARSE_CLOCK_H