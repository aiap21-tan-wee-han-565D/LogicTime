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
    data->LS = (int*)calloc(n, sizeof(int));  // LS[j] = v[pid] when last sent to process j
    data->LU = (int*)calloc(n, sizeof(int));  // LU[k] = v[pid] when entry k was last updated
    
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
        if (data->LS) {
            free(data->LS);
        }
        if (data->LU) {
            free(data->LU);
        }
        free(ts->data);
        ts->data = NULL;
    }
}

void differential_increment(Timestamp *ts) {
    DifferentialClockData *data = (DifferentialClockData*)ts->data;
    data->v[ts->pid] += 1;
    data->LU[ts->pid] = data->v[ts->pid]; // Update LU when this process's entry is modified
}

void differential_merge(Timestamp *dst, const void *other_data, size_t other_size) {
    DifferentialClockData *dst_data = (DifferentialClockData*)dst->data;
    
    if (other_size == dst->n * sizeof(int)) {
        // Full vector format (for compatibility)
        const int *other_v = (const int*)other_data;
        for (int i = 0; i < dst->n; i++) {
            if (other_v[i] > dst_data->v[i]) {
                dst_data->v[i] = other_v[i];
                // Update LU[i] = vt[j] + 1 (next logical time when entry i will be updated)
                dst_data->LU[i] = dst_data->v[dst->pid] + 1;
            }
        }
    } else {
        // Differential format: pairs of (process_id, value)
        const int *buf = (const int*)other_data;
        int pair_count = other_size / (2 * sizeof(int));
        
        for (int i = 0; i < pair_count; i++) {
            int k = buf[i * 2];       // process id
            int val = buf[i * 2 + 1]; // value
            
            if (k >= 0 && k < dst->n && val > dst_data->v[k]) {
                dst_data->v[k] = val;
                // Update LU[k] = vt[j] + 1 (next logical time when entry k will be updated)
                dst_data->LU[k] = dst_data->v[dst->pid] + 1;
            }
        }
    }
    
    // Increment own vector clock last (the receive event)
    dst_data->v[dst->pid]++;
    dst_data->LU[dst->pid] = dst_data->v[dst->pid];
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
// takes destination into account - implements true Singhal-Kshemkalyani algorithm
size_t differential_serialize_for_dest(const Timestamp *ts, int dest, void *buffer, size_t bufsize) {
    DifferentialClockData *data = (DifferentialClockData*)ts->data;
    
    // Calculate which entries to send: {(k, v[k]) | LS[dest] < LU[k] or k = pid}
    int send_count = 0;
    for (int k = 0; k < ts->n; k++) {
        if (data->LS[dest] < data->LU[k] || k == ts->pid) {
            send_count++;
        }
    }
    
    // Store as pairs of (process_id, value)
    size_t required = send_count * 2 * sizeof(int);
    
    if (bufsize >= required) {
        int *buf = (int*)buffer;
        int idx = 0;
        for (int k = 0; k < ts->n; k++) {
            if (data->LS[dest] < data->LU[k] || k == ts->pid) {
                buf[idx++] = k;                // process id
                buf[idx++] = data->v[k];       // current value
            }
        }
        // Update LS[dest] = current vector time after successful serialization
        data->LS[dest] = data->v[ts->pid];
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
        // Full vector format - update entire vector
        const int *full_v = (const int*)buffer;
        for (int i = 0; i < ts->n; i++) {
            if (full_v[i] > data->v[i]) {
                data->v[i] = full_v[i];
                // Update LU for changed components (next logical time)
                data->LU[i] = data->v[ts->pid] + 1;
            }
        }
    } else {
        // Differential format: pairs of (process_id, value)
        const int *buf = (const int*)buffer;
        int pair_count = size / (2 * sizeof(int));
        
        for (int i = 0; i < pair_count; i++) {
            int pid = buf[i * 2]; // Extracts pid from even index 
            int value = buf[i * 2 + 1]; // Extracts value from odd index
            if (pid >= 0 && pid < ts->n) {
                if (value > data->v[pid]) {
                    data->v[pid] = value;
                    // Update LU when component is updated (next logical time)
                    data->LU[pid] = data->v[ts->pid] + 1;
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
    memcpy(dst_data->LS, src_data->LS, ts->n * sizeof(int));
    memcpy(dst_data->LU, src_data->LU, ts->n * sizeof(int));
    
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
    .serialize_for_dest = differential_serialize_for_dest,
    .deserialize = differential_deserialize,
    .to_string = differential_to_string,
    .clone = differential_clone
};