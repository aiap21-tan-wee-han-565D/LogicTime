#ifndef ENCODED_CLOCK_H
#define ENCODED_CLOCK_H

#include "timestamp.h"

/* ---------- Prime Numbers for Encoded Clocks ---------- */

extern int PRIMES[];
extern const int MAX_PRIMES;

/* ---------- Encoded Vector Clock Data Structure ---------- */

// Encoded clock data
typedef struct {
    unsigned long long value;  // encoded timestamp
    int overflow;              // overflow flag
    int *fallback_v;           // fallback vector on overflow
} EncodedClockData;

/* ---------- Encoded Vector Clock Operations ---------- */

Timestamp encoded_create(int n, int pid, ClockType type);
void encoded_destroy(Timestamp *ts);
void encoded_increment(Timestamp *ts);
void encoded_merge(Timestamp *dst, const void *other_data, size_t other_size);
TSOrder encoded_compare(const Timestamp *a, const Timestamp *b);
size_t encoded_serialize(const Timestamp *ts, void *buffer, size_t bufsize);
void encoded_deserialize(Timestamp *ts, const void *buffer, size_t size);
void encoded_to_string(const Timestamp *ts, char *buf, size_t bufsize);
Timestamp encoded_clone(const Timestamp *ts);

/* ---------- Operations Table ---------- */

extern TimestampOps ENCODED_OPS;

#endif // ENCODED_CLOCK_H