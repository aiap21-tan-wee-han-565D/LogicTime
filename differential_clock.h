#ifndef DIFFERENTIAL_CLOCK_H
#define DIFFERENTIAL_CLOCK_H

#include "timestamp.h"

/* ---------- Differential Vector Clock Data Structure ---------- */

// Differential clock data (Singhal-Kshemkalyani technique)
typedef struct {
    int *v;                    // current vector clock
    int **last_sent;           // last vector sent to each process
} DifferentialClockData;

/* ---------- Differential Vector Clock Operations ---------- */

Timestamp differential_create(int n, int pid, ClockType type);
void differential_destroy(Timestamp *ts);
void differential_increment(Timestamp *ts);
void differential_merge(Timestamp *dst, const void *other_data, size_t other_size);
TSOrder differential_compare(const Timestamp *a, const Timestamp *b);
size_t differential_serialize(const Timestamp *ts, void *buffer, size_t bufsize);
void differential_deserialize(Timestamp *ts, const void *buffer, size_t size);
void differential_to_string(const Timestamp *ts, char *buf, size_t bufsize);
Timestamp differential_clone(const Timestamp *ts);

/* ---------- Special Functions for Differential Technique ---------- */

// For differential technique, we need a special serialize function that
// takes destination into account
size_t differential_serialize_for_dest(const Timestamp *ts, int dest, void *buffer, size_t bufsize);

/* ---------- Operations Table ---------- */

extern TimestampOps DIFFERENTIAL_OPS;

#endif // DIFFERENTIAL_CLOCK_H