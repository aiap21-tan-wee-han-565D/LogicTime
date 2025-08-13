#include <stdio.h>
#include <stdlib.h>
#include "timestamp.h"
#include "standard_clock.h"
#include "sparse_clock.h"
#include "differential_clock.h"
#include "encoded_clock.h"

/* ---------- Clock Type Information ---------- */

const char* clock_type_names[] = {
    "Standard", "Sparse", "Differential", "Encoded"
};

const char* clock_type_descriptions[] = {
    "Full vector clocks (baseline)",
    "Sparse representation (only non-zero entries)",
    "Differential technique (Singhal-Kshemkalyani)",
    "Prime number encoding (single integer)"
};

/* ---------- Operations Dispatch ---------- */

static TimestampOps* get_ops(ClockType type) {
    switch (type) {
        case CLOCK_STANDARD: return &STANDARD_OPS;
        case CLOCK_SPARSE: return &SPARSE_OPS;
        case CLOCK_DIFFERENTIAL: return &DIFFERENTIAL_OPS;
        case CLOCK_ENCODED: return &ENCODED_OPS;
        default:
            fprintf(stderr, "Unknown clock type: %d\n", type);
            exit(1);
    }
}

/* ---------- Main Timestamp Interface Implementation ---------- */

Timestamp ts_create(int n, int pid, ClockType type) {
    return get_ops(type)->create(n, pid, type);
}

void ts_destroy(Timestamp *ts) {
    get_ops(ts->type)->destroy(ts);
}

void ts_increment(Timestamp *ts) {
    get_ops(ts->type)->increment(ts);
}

void ts_merge(Timestamp *dst, const void *other_data, size_t other_size) {
    get_ops(dst->type)->merge(dst, other_data, other_size);
}

TSOrder ts_compare(const Timestamp *a, const Timestamp *b) {
    if (a->type != b->type) {
        fprintf(stderr, "Cannot compare different clock types!\n");
        exit(1);
    }
    return get_ops(a->type)->compare(a, b);
}

size_t ts_serialize(const Timestamp *ts, void *buffer, size_t bufsize) {
    return get_ops(ts->type)->serialize(ts, buffer, bufsize);
}

void ts_deserialize(Timestamp *ts, const void *buffer, size_t size) {
    get_ops(ts->type)->deserialize(ts, buffer, size);
}

void ts_to_string(const Timestamp *ts, char *buf, size_t bufsize) {
    get_ops(ts->type)->to_string(ts, buf, bufsize);
}

Timestamp ts_clone(const Timestamp *ts) {
    return get_ops(ts->type)->clone(ts);
}