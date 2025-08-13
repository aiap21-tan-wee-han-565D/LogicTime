#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "differential_clock.h"

/* ---------- Differential Vector Clock Implementation (Singhal-Kshemkalyani) ---------- */

Timestamp differential_create(int n, int pid, ClockType type) {
    Timestamp ts;
    ts.n = n;
    ts.pid = pid;
    ts.type = type;
    
    DifferentialClockData *data = malloc(sizeof(DifferentialClockData));
    data->v = (int*)calloc(n, sizeof(int));
    data->last_sent = (int**)malloc(n * sizeof(int*));
    for (int i = 0; i < n; i++) {
        data->last_sent[i] = (int*)calloc(n, sizeof(int));
    }
    
    ts.data = data;
    ts.data_size = 0; // Dynamic size based on differences
    return ts;
}

void differential_destroy(Timestamp *ts) {
    if (ts && ts->data) {
        DifferentialClockData *data = (DifferentialClockData*)ts->data;
        if (data->v) {
            free(data->v);
        }
        if (data->last_sent) {
            for (int i = 0; i < ts->n; i++) {
                if (data->last_sent[i]) {
                    free(data->last_sent[i]);
                }
            }
            free(data->last_sent);
        }
        free(ts->data);
        ts->data = NULL;
    }
}

void differential_increment(Timestamp *ts) {
    DifferentialClockData *data = (DifferentialClockData*)ts->data;
    data->v[ts->pid] += 1;
}

void differential_merge(Timestamp *dst, const void *other_data, size_t other_size) {
    DifferentialClockData *dst_data = (DifferentialClockData*)dst->data;
    const int *other_v = (const int*)other_data;
    
    // This is the full vector from the message, merge it
    for (int i = 0; i < dst->n; i++) {
        if (other_v[i] > dst_data->v[i]) {
            dst_data->v[i] = other_v[i];
        }
    }
}

TSOrder differential_compare(const Timestamp *a, const Timestamp *b) {
    const DifferentialClockData *a_data = (const DifferentialClockData*)a->data;
    const DifferentialClockData *b_data = (const DifferentialClockData*)b->data;
    
    // Compare the underlying vectors
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

// For differential technique, we need a special serialize function that
// takes destination into account
size_t differential_serialize_for_dest(const Timestamp *ts, int dest, void *buffer, size_t bufsize) {
    const DifferentialClockData *data = (const DifferentialClockData*)ts->data;
    
    // Calculate differences since last send to this destination
    int diff_count = 0;
    for (int i = 0; i < ts->n; i++) {
        if (data->v[i] != data->last_sent[dest][i]) {
            diff_count++;
        }
    }
    
    // Store as pairs of (process_id, new_value)
    size_t required = diff_count * 2 * sizeof(int);
    
    if (bufsize >= required) {
        int *buf = (int*)buffer;
        int idx = 0;
        for (int i = 0; i < ts->n; i++) {
            if (data->v[i] != data->last_sent[dest][i]) {
                buf[idx++] = i;                // process id
                buf[idx++] = data->v[i];       // new value
                // Update last_sent
                data->last_sent[dest][i] = data->v[i];
            }
        }
    }
    
    return required;
}

size_t differential_serialize(const Timestamp *ts, void *buffer, size_t bufsize) {
    // For compatibility, serialize full vector
    const DifferentialClockData *data = (const DifferentialClockData*)ts->data;
    size_t required = ts->n * sizeof(int);
    
    if (bufsize >= required) {
        memcpy(buffer, data->v, required);
    }
    return required;
}

void differential_deserialize(Timestamp *ts, const void *buffer, size_t size) {
    DifferentialClockData *data = (DifferentialClockData*)ts->data;
    
    if (size == ts->n * sizeof(int)) {
        // Full vector
        memcpy(data->v, buffer, size);
    } else {
        // Differential format: pairs of (process_id, value)
        const int *buf = (const int*)buffer;
        int pair_count = size / (2 * sizeof(int));
        
        for (int i = 0; i < pair_count; i++) {
            int pid = buf[i * 2];
            int value = buf[i * 2 + 1];
            if (pid >= 0 && pid < ts->n) {
                if (value > data->v[pid]) {
                    data->v[pid] = value;
                }
            }
        }
    }
}

void differential_to_string(const Timestamp *ts, char *buf, size_t bufsize) {
    const DifferentialClockData *data = (const DifferentialClockData*)ts->data;
    size_t used = 0;
    
    used += snprintf(buf + used, bufsize - used, "D[");
    for (int i = 0; i < ts->n; i++) {
        used += snprintf(buf + used, bufsize - used, "%s%d", 
                        (i ? "," : ""), data->v[i]);
        if (used >= bufsize) break;
    }
    snprintf(buf + used, bufsize - used, "]");
}

Timestamp differential_clone(const Timestamp *ts) {
    Timestamp out = differential_create(ts->n, ts->pid, ts->type);
    const DifferentialClockData *src_data = (const DifferentialClockData*)ts->data;
    DifferentialClockData *dst_data = (DifferentialClockData*)out.data;
    
    memcpy(dst_data->v, src_data->v, ts->n * sizeof(int));
    for (int i = 0; i < ts->n; i++) {
        memcpy(dst_data->last_sent[i], src_data->last_sent[i], ts->n * sizeof(int));
    }
    
    return out;
}

/* ---------- Operations Table ---------- */

TimestampOps DIFFERENTIAL_OPS = {
    .create = differential_create,
    .destroy = differential_destroy,
    .increment = differential_increment,
    .merge = differential_merge,
    .compare = differential_compare,
    .serialize = differential_serialize,
    .deserialize = differential_deserialize,
    .to_string = differential_to_string,
    .clone = differential_clone
};