#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compressed_clock.h"

/* ---------- Compressed Vector Clock Implementation (True Delta Compression) ---------- */

Timestamp compressed_create(int n, int pid, ClockType type) {
    Timestamp ts;
    ts.n = n;
    ts.pid = pid;
    ts.type = type;
    
    CompressedClockData *data = malloc(sizeof(CompressedClockData));
    data->n = n;
    
    // Allocate current vector clock
    data->vt = (int*)calloc(n, sizeof(int));
    
    // Allocate tau matrix: tau[j][k] = last timestamp sent to receiver j
    data->tau = (int**)malloc(n * sizeof(int*));
    for (int j = 0; j < n; j++) {
        data->tau[j] = (int*)calloc(n, sizeof(int));  // Initialize to [0,0,...,0] for all receivers
    }
    
    ts.data = data;
    ts.data_size = 0; // Dynamic size based on compression
    return ts;
}

void compressed_destroy(Timestamp *ts) {
    if (ts && ts->data) {
        CompressedClockData *data = (CompressedClockData*)ts->data;
        
        // Free vector clock
        if (data->vt) {
            free(data->vt);
        }
        
        // Free tau matrix
        if (data->tau) {
            for (int j = 0; j < data->n; j++) {
                if (data->tau[j]) {
                    free(data->tau[j]);
                }
            }
            free(data->tau);
        }
        
        free(ts->data);
        ts->data = NULL;
    }
}

void compressed_increment(Timestamp *ts) {
    CompressedClockData *data = (CompressedClockData*)ts->data;
    data->vt[ts->pid]++;
}

void compressed_merge(Timestamp *dst, const void *other_data, size_t other_size) {
    CompressedClockData *dst_data = (CompressedClockData*)dst->data;
    
    if (other_size == dst->n * sizeof(int)) {
        // Full vector format (for compatibility with other clock types)
        const int *other_vt = (const int*)other_data;
        for (int i = 0; i < dst->n; i++) {
            if (other_vt[i] > dst_data->vt[i]) {
                dst_data->vt[i] = other_vt[i];
            }
        }
    } else {
        // Compressed format: [count, (index1, value1), (index2, value2), ...]
        const int *buf = (const int*)other_data;
        if (other_size >= sizeof(int)) {
            int count = buf[0];
            
            // Ensure we have enough data for all pairs
            if (other_size >= (1 + 2 * count) * sizeof(int)) {
                for (int i = 0; i < count; i++) {
                    int index = buf[1 + i * 2];
                    int value = buf[1 + i * 2 + 1];
                    
                    if (index >= 0 && index < dst->n && value > dst_data->vt[index]) {
                        dst_data->vt[index] = value;
                    }
                }
            }
        }
    }
    
    // Increment local clock after merge (handles increment internally like differential clocks)
    dst_data->vt[dst->pid]++;
}

TSOrder compressed_compare(const Timestamp *a, const Timestamp *b) {
    const CompressedClockData *a_data = (const CompressedClockData*)a->data;
    const CompressedClockData *b_data = (const CompressedClockData*)b->data;
    
    // Compare the underlying vector clocks
    int a_le_b = 1, b_le_a = 1;
    int a_lt_b = 0, b_lt_a = 0;
    
    for (int i = 0; i < a->n; i++) {
        if (a_data->vt[i] > b_data->vt[i]) {
            a_le_b = 0;
            b_lt_a = 1;
        }
        if (b_data->vt[i] > a_data->vt[i]) {
            b_le_a = 0;
            a_lt_b = 1;
        }
    }
    
    if (a_le_b && b_le_a) return TS_EQUAL;
    if (a_le_b && a_lt_b) return TS_BEFORE;
    if (b_le_a && b_lt_a) return TS_AFTER;
    return TS_CONCURRENT;
}

// Core compression algorithm - implements the exact algorithm described
size_t compressed_serialize_for_dest(const Timestamp *ts, int dest, void *buffer, size_t bufsize) {
    CompressedClockData *data = (CompressedClockData*)ts->data;
    
    // Note: Clock increment is handled by simulation framework before this call
    // Step 1: Find the diffs - compare current vt with tau[dest]
    int diff_count = 0;
    for (int k = 0; k < data->n; k++) {
        if (data->tau[dest][k] != data->vt[k]) {
            diff_count++;
        }
    }
    
    // Step 2: Calculate size for compressed vs full format
    size_t compressed_size = (1 + 2 * diff_count) * sizeof(int);
    size_t full_size = data->n * sizeof(int);
    
    // Use compression only if it's actually smaller or equal
    if (compressed_size <= full_size && diff_count > 0) {
        // Use compressed format
        if (bufsize >= compressed_size) {
            int *buf = (int*)buffer;
            buf[0] = diff_count;  // Number of changed entries
            
            int buf_idx = 1;
            for (int k = 0; k < data->n; k++) {
                if (data->tau[dest][k] != data->vt[k]) {
                    buf[buf_idx++] = k;                // index
                    buf[buf_idx++] = data->vt[k];      // current value
                }
            }
            
            // Step 3: Remember what you sent - set tau[dest] := vt
            memcpy(data->tau[dest], data->vt, data->n * sizeof(int));
        }
        return compressed_size;
    } else {
        // Use full format (more efficient)
        if (bufsize >= full_size) {
            memcpy(buffer, data->vt, full_size);
            // Step 3: Remember what you sent - set tau[dest] := vt
            memcpy(data->tau[dest], data->vt, data->n * sizeof(int));
        }
        return full_size;
    }
}

size_t compressed_serialize(const Timestamp *ts, void *buffer, size_t bufsize) {
    // For compatibility, serialize full vector
    const CompressedClockData *data = (const CompressedClockData*)ts->data;
    size_t required = ts->n * sizeof(int);
    
    if (bufsize >= required) {
        memcpy(buffer, data->vt, required);
    }
    return required;
}

void compressed_deserialize(Timestamp *ts, const void *buffer, size_t size) {
    CompressedClockData *data = (CompressedClockData*)ts->data;
    
    if (size == ts->n * sizeof(int)) {
        // Full vector format
        const int *full_vt = (const int*)buffer;
        for (int i = 0; i < ts->n; i++) {
            if (full_vt[i] > data->vt[i]) {
                data->vt[i] = full_vt[i];
            }
        }
    } else {
        // Compressed format: [count, (index1, value1), (index2, value2), ...]
        const int *buf = (const int*)buffer;
        if (size >= sizeof(int)) {
            int count = buf[0];
            
            if (size >= (1 + 2 * count) * sizeof(int)) {
                for (int i = 0; i < count; i++) {
                    int index = buf[1 + i * 2];
                    int value = buf[1 + i * 2 + 1];
                    
                    if (index >= 0 && index < ts->n && value > data->vt[index]) {
                        data->vt[index] = value;
                    }
                }
            }
        }
    }
}

void compressed_to_string(const Timestamp *ts, char *buf, size_t bufsize) {
    const CompressedClockData *data = (const CompressedClockData*)ts->data;
    size_t used = 0;
    
    used += snprintf(buf + used, bufsize - used, "C[");
    for (int i = 0; i < ts->n; i++) {
        used += snprintf(buf + used, bufsize - used, "%s%d", 
                        (i ? "," : ""), data->vt[i]);
        if (used >= bufsize) break;
    }
    snprintf(buf + used, bufsize - used, "]");
}

Timestamp compressed_clone(const Timestamp *ts) {
    Timestamp out = compressed_create(ts->n, ts->pid, ts->type);
    const CompressedClockData *src_data = (const CompressedClockData*)ts->data;
    CompressedClockData *dst_data = (CompressedClockData*)out.data;
    
    // Copy vector clock
    memcpy(dst_data->vt, src_data->vt, ts->n * sizeof(int));
    
    // Copy tau matrix
    for (int j = 0; j < ts->n; j++) {
        memcpy(dst_data->tau[j], src_data->tau[j], ts->n * sizeof(int));
    }
    
    return out;
}

/* ---------- Operations Table ---------- */

TimestampOps COMPRESSED_OPS = {
    .create = compressed_create,
    .destroy = compressed_destroy,
    .increment = compressed_increment,
    .merge = compressed_merge,
    .compare = compressed_compare,
    .serialize = compressed_serialize,
    .serialize_for_dest = compressed_serialize_for_dest,
    .deserialize = compressed_deserialize,
    .to_string = compressed_to_string,
    .clone = compressed_clone
};