#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sparse_clock.h"

/* ---------- Sparse Vector Clock Implementation ---------- */

Timestamp sparse_create(int n, int pid, ClockType type) {
    Timestamp ts;
    ts.n = n;
    ts.pid = pid;
    ts.type = type;
    
    SparseClockData *data = malloc(sizeof(SparseClockData));
    data->entries = malloc(n * sizeof(SparseEntry));
    data->count = 0;
    data->capacity = n;
    
    ts.data = data;
    ts.data_size = 0; // Dynamic size
    return ts;
}

void sparse_destroy(Timestamp *ts) {
    if (ts && ts->data) {
        SparseClockData *data = (SparseClockData*)ts->data;
        if (data->entries) {
            free(data->entries);
            data->entries = NULL;
        }
        free(ts->data);
        ts->data = NULL;
    }
}

void sparse_increment(Timestamp *ts) {
    SparseClockData *data = (SparseClockData*)ts->data;
    
    // Find entry for this process
    for (int i = 0; i < data->count; i++) {
        if (data->entries[i].pid == ts->pid) {
            data->entries[i].counter++;
            return;
        }
    }
    
    // Add new entry
    if (data->count >= data->capacity) {
        data->capacity *= 2;
        data->entries = realloc(data->entries, data->capacity * sizeof(SparseEntry));
    }
    data->entries[data->count].pid = ts->pid;
    data->entries[data->count].counter = 1;
    data->count++;
}

void sparse_merge(Timestamp *dst, const void *other_data, size_t other_size) {
    SparseClockData *dst_data = (SparseClockData*)dst->data;
    const SparseEntry *other_entries = (const SparseEntry*)other_data;
    int other_count = other_size / sizeof(SparseEntry);
    
    for (int i = 0; i < other_count; i++) {
        int other_pid = other_entries[i].pid;
        int other_counter = other_entries[i].counter;
        
        // Find corresponding entry in destination
        int found = 0;
        for (int j = 0; j < dst_data->count; j++) {
            if (dst_data->entries[j].pid == other_pid) {
                if (other_counter > dst_data->entries[j].counter) {
                    dst_data->entries[j].counter = other_counter;
                }
                found = 1;
                break;
            }
        }
        
        if (!found) {
            // Add new entry
            if (dst_data->count >= dst_data->capacity) {
                dst_data->capacity *= 2;
                dst_data->entries = realloc(dst_data->entries, 
                    dst_data->capacity * sizeof(SparseEntry));
            }
            dst_data->entries[dst_data->count].pid = other_pid;
            dst_data->entries[dst_data->count].counter = other_counter;
            dst_data->count++;
        }
    }
}

TSOrder sparse_compare(const Timestamp *a, const Timestamp *b) {
    const SparseClockData *a_data = (const SparseClockData*)a->data;
    const SparseClockData *b_data = (const SparseClockData*)b->data;
    
    int a_le_b = 1, b_le_a = 1;
    int a_lt_b = 0, b_lt_a = 0;
    
    // Check all processes in both clocks
    for (int pid = 0; pid < a->n; pid++) {
        int a_val = 0, b_val = 0;
        
        // Get value for process pid in clock a
        for (int i = 0; i < a_data->count; i++) {
            if (a_data->entries[i].pid == pid) {
                a_val = a_data->entries[i].counter;
                break;
            }
        }
        
        // Get value for process pid in clock b
        for (int i = 0; i < b_data->count; i++) {
            if (b_data->entries[i].pid == pid) {
                b_val = b_data->entries[i].counter;
                break;
            }
        }
        
        if (a_val > b_val) {
            a_le_b = 0;
            b_lt_a = 1;
        }
        if (b_val > a_val) {
            b_le_a = 0;
            a_lt_b = 1;
        }
    }
    
    if (a_le_b && b_le_a) return TS_EQUAL;
    if (a_le_b && a_lt_b) return TS_BEFORE;
    if (b_le_a && b_lt_a) return TS_AFTER;
    return TS_CONCURRENT;
}

size_t sparse_serialize(const Timestamp *ts, void *buffer, size_t bufsize) {
    const SparseClockData *data = (const SparseClockData*)ts->data;
    size_t required = data->count * sizeof(SparseEntry);
    
    if (bufsize >= required) {
        memcpy(buffer, data->entries, required);
    }
    return required;
}

void sparse_deserialize(Timestamp *ts, const void *buffer, size_t size) {
    SparseClockData *data = (SparseClockData*)ts->data;
    int count = size / sizeof(SparseEntry);
    
    if (count > data->capacity) {
        data->capacity = count;
        data->entries = realloc(data->entries, data->capacity * sizeof(SparseEntry));
    }
    
    memcpy(data->entries, buffer, size);
    data->count = count;
}

void sparse_to_string(const Timestamp *ts, char *buf, size_t bufsize) {
    const SparseClockData *data = (const SparseClockData*)ts->data;
    size_t used = 0;
    
    used += snprintf(buf + used, bufsize - used, "{%d:", data->count);
    for (int i = 0; i < data->count; i++) {
        used += snprintf(buf + used, bufsize - used, "%sP%d:%d", 
                        (i ? "," : ""), data->entries[i].pid, data->entries[i].counter);
        if (used >= bufsize) break;
    }
    snprintf(buf + used, bufsize - used, "}");
}

Timestamp sparse_clone(const Timestamp *ts) {
    Timestamp out = sparse_create(ts->n, ts->pid, ts->type);
    const SparseClockData *src_data = (const SparseClockData*)ts->data;
    SparseClockData *dst_data = (SparseClockData*)out.data;
    
    if (src_data->count > 0) {
        dst_data->entries = realloc(dst_data->entries, 
            src_data->count * sizeof(SparseEntry));
        memcpy(dst_data->entries, src_data->entries, 
            src_data->count * sizeof(SparseEntry));
        dst_data->count = src_data->count;
        dst_data->capacity = src_data->count;
    }
    return out;
}

/* ---------- Operations Table ---------- */

TimestampOps SPARSE_OPS = {
    .create = sparse_create,
    .destroy = sparse_destroy,
    .increment = sparse_increment,
    .merge = sparse_merge,
    .compare = sparse_compare,
    .serialize = sparse_serialize,
    .serialize_for_dest = NULL,  // Sparse clocks don't need destination-aware serialization
    .deserialize = sparse_deserialize,
    .to_string = sparse_to_string,
    .clone = sparse_clone
};