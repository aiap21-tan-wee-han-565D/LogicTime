#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "standard_clock.h"

/* ---------- Standard Vector Clock Implementation ---------- */

Timestamp standard_create(int n, int pid, ClockType type) {
    Timestamp ts;
    ts.n = n;
    ts.pid = pid;
    ts.type = type;
    
    StandardClockData *data = malloc(sizeof(StandardClockData));
    data->v = (int*)calloc(n, sizeof(int));
    if (!data->v) {
        fprintf(stderr, "OOM\n");
        exit(1);
    }
    
    ts.data = data;
    ts.data_size = n * sizeof(int);
    return ts;
}

void standard_destroy(Timestamp *ts) {
    if (ts && ts->data) {
        StandardClockData *data = (StandardClockData*)ts->data;
        if (data->v) {
            free(data->v);
            data->v = NULL;
        }
        free(ts->data);
        ts->data = NULL;
    }
}

void standard_increment(Timestamp *ts) {
    StandardClockData *data = (StandardClockData*)ts->data;
    data->v[ts->pid] += 1;
}

void standard_merge(Timestamp *dst, const void *other_data, size_t other_size) {
    StandardClockData *dst_data = (StandardClockData*)dst->data;
    const int *other_v = (const int*)other_data;
    
    for (int i = 0; i < dst->n; i++) {
        if (other_v[i] > dst_data->v[i]) {
            dst_data->v[i] = other_v[i];
        }
    }
}

TSOrder standard_compare(const Timestamp *a, const Timestamp *b) {
    if (a->n != b->n) {
        fprintf(stderr, "Mismatched vector sizes!\n");
        exit(1);
    }
    
    const StandardClockData *a_data = (const StandardClockData*)a->data;
    const StandardClockData *b_data = (const StandardClockData*)b->data;
    
    int a_le_b = 1, b_le_a = 1;
    int a_lt_b = 0, b_lt_a = 0;
    
    for (int i = 0; i < a->n; i++) {
        if (a_data->v[i] > b_data->v[i]) {
            a_le_b = 0;
            b_lt_a = 1;
        }
        if (b_data->v[i] > a_data->v[i]) {
            b_le_a = 0;
            a_lt_b = 1;
        }
    }
    
    if (a_le_b && b_le_a) return TS_EQUAL;
    if (a_le_b && a_lt_b) return TS_BEFORE;
    if (b_le_a && b_lt_a) return TS_AFTER;
    return TS_CONCURRENT;
}

size_t standard_serialize(const Timestamp *ts, void *buffer, size_t bufsize) {
    const StandardClockData *data = (const StandardClockData*)ts->data;
    size_t required = ts->n * sizeof(int);
    
    if (bufsize >= required) {
        memcpy(buffer, data->v, required);
    }
    return required;
}

void standard_deserialize(Timestamp *ts, const void *buffer, size_t size) {
    StandardClockData *data = (StandardClockData*)ts->data;
    size_t expected = ts->n * sizeof(int);
    
    if (size == expected) {
        memcpy(data->v, buffer, size);
    }
}

void standard_to_string(const Timestamp *ts, char *buf, size_t bufsize) {
    const StandardClockData *data = (const StandardClockData*)ts->data;
    size_t used = 0;
    
    used += snprintf(buf + used, bufsize - used, "[");
    for (int i = 0; i < ts->n; i++) {
        used += snprintf(buf + used, bufsize - used, "%s%d", 
                        (i ? "," : ""), data->v[i]);
        if (used >= bufsize) break;
    }
    snprintf(buf + used, bufsize - used, "]");
}

Timestamp standard_clone(const Timestamp *ts) {
    Timestamp out = standard_create(ts->n, ts->pid, ts->type);
    const StandardClockData *src_data = (const StandardClockData*)ts->data;
    StandardClockData *dst_data = (StandardClockData*)out.data;
    
    memcpy(dst_data->v, src_data->v, ts->n * sizeof(int));
    return out;
}

/* ---------- Operations Table ---------- */

TimestampOps STANDARD_OPS = {
    .create = standard_create,
    .destroy = standard_destroy,
    .increment = standard_increment,
    .merge = standard_merge,
    .compare = standard_compare,
    .serialize = standard_serialize,
    .serialize_for_dest = NULL,  // Standard clocks don't need destination-aware serialization
    .deserialize = standard_deserialize,
    .to_string = standard_to_string,
    .clone = standard_clone
};