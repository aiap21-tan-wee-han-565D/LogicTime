#ifndef COMPRESSED_CLOCK_H
#define COMPRESSED_CLOCK_H

#include "timestamp.h"

/* ---------- Compressed Vector Clock Data Structure ---------- */

// Compressed vector clock data (True Delta Compression)
typedef struct {
    int *vt;                   // Current vector clock [n]
    int **tau;                 // Last sent timestamps tau[j][k] - what was last sent to each receiver j
    int n;                     // Number of processes (for convenience)
} CompressedClockData;

/* ---------- Compressed Vector Clock Operations ---------- */

Timestamp compressed_create(int n, int pid, ClockType type);
void compressed_destroy(Timestamp *ts);
void compressed_increment(Timestamp *ts);
void compressed_merge(Timestamp *dst, const void *other_data, size_t other_size);
TSOrder compressed_compare(const Timestamp *a, const Timestamp *b);
size_t compressed_serialize(const Timestamp *ts, void *buffer, size_t bufsize);
void compressed_deserialize(Timestamp *ts, const void *buffer, size_t size);
void compressed_to_string(const Timestamp *ts, char *buf, size_t bufsize);
Timestamp compressed_clone(const Timestamp *ts);

/* ---------- Special Functions for Compressed Technique ---------- */

// Destination-aware serialization - core of the compression algorithm
size_t compressed_serialize_for_dest(const Timestamp *ts, int dest, void *buffer, size_t bufsize);

/* ---------- Operations Table ---------- */

extern TimestampOps COMPRESSED_OPS;

#endif // COMPRESSED_CLOCK_H