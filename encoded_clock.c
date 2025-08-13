#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "encoded_clock.h"

/* ---------- Prime Numbers for Encoded Clocks ---------- */

int PRIMES[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};
const int MAX_PRIMES = 25;

/* ---------- Encoded Vector Clock Implementation (Prime Numbers) ---------- */

Timestamp encoded_create(int n, int pid, ClockType type) {
    if (n > MAX_PRIMES) {
        fprintf(stderr, "Too many processes for encoded clock (max %d)\n", MAX_PRIMES);
        exit(1);
    }
    
    Timestamp ts;
    ts.n = n;
    ts.pid = pid;
    ts.type = type;
    
    EncodedClockData *data = malloc(sizeof(EncodedClockData));
    data->value = 1;  // Start with 1 (multiplicative identity)
    data->overflow = 0;
    data->fallback_v = (int*)calloc(n, sizeof(int));
    
    ts.data = data;
    ts.data_size = sizeof(unsigned long long);
    return ts;
}

void encoded_destroy(Timestamp *ts) {
    if (ts && ts->data) {
        EncodedClockData *data = (EncodedClockData*)ts->data;
        if (data->fallback_v) {
            free(data->fallback_v);
        }
        free(ts->data);
        ts->data = NULL;
    }
}

void encoded_increment(Timestamp *ts) {
    EncodedClockData *data = (EncodedClockData*)ts->data;
    
    if (data->overflow) {
        // Use fallback vector
        data->fallback_v[ts->pid] += 1;
        return;
    }
    
    // Try to multiply by the prime for this process
    unsigned long long new_value = data->value * PRIMES[ts->pid];
    
    // Check for overflow
    if (new_value / PRIMES[ts->pid] != data->value) {
        // Overflow detected, switch to fallback
        data->overflow = 1;
        // Convert current encoded value to vector
        unsigned long long temp = data->value;
        for (int i = 0; i < ts->n; i++) {
            while (temp % PRIMES[i] == 0) {
                data->fallback_v[i]++;
                temp /= PRIMES[i];
            }
        }
        data->fallback_v[ts->pid] += 1;
    } else {
        data->value = new_value;
    }
}

void encoded_merge(Timestamp *dst, const void *other_data, size_t other_size) {
    EncodedClockData *dst_data = (EncodedClockData*)dst->data;
    
    if (other_size == sizeof(unsigned long long)) {
        // Other is encoded format
        unsigned long long other_value = *(const unsigned long long*)other_data;
        
        if (dst_data->overflow) {
            // We're in fallback mode, decode the other value
            int other_v[MAX_PRIMES];
            memset(other_v, 0, sizeof(other_v));
            unsigned long long temp = other_value;
            for (int i = 0; i < dst->n; i++) {
                while (temp % PRIMES[i] == 0) {
                    other_v[i]++;
                    temp /= PRIMES[i];
                }
            }
            
            // Merge with fallback vector
            for (int i = 0; i < dst->n; i++) {
                if (other_v[i] > dst_data->fallback_v[i]) {
                    dst_data->fallback_v[i] = other_v[i];
                }
            }
        } else {
            // Both are encoded, take the LCM (but this is complex)
            // For simplicity, decode both and merge
            int dst_v[MAX_PRIMES];
            int other_v[MAX_PRIMES];
            memset(dst_v, 0, sizeof(dst_v));
            memset(other_v, 0, sizeof(other_v));
            
            // Decode dst
            unsigned long long temp = dst_data->value;
            for (int i = 0; i < dst->n; i++) {
                while (temp % PRIMES[i] == 0) {
                    dst_v[i]++;
                    temp /= PRIMES[i];
                }
            }
            
            // Decode other
            temp = other_value;
            for (int i = 0; i < dst->n; i++) {
                while (temp % PRIMES[i] == 0) {
                    other_v[i]++;
                    temp /= PRIMES[i];
                }
            }
            
            // Merge vectors
            for (int i = 0; i < dst->n; i++) {
                if (other_v[i] > dst_v[i]) {
                    dst_v[i] = other_v[i];
                }
            }
            
            // Try to re-encode
            unsigned long long result = 1;
            int overflow_check = 0;
            
            for (int i = 0; i < dst->n && !overflow_check; i++) {
                for (int j = 0; j < dst_v[i]; j++) {
                    unsigned long long new_result = result * PRIMES[i];
                    if (new_result / PRIMES[i] != result) {
                        overflow_check = 1;
                        break;
                    }
                    result = new_result;
                }
            }
            
            if (overflow_check) {
                // Switch to fallback
                dst_data->overflow = 1;
                memcpy(dst_data->fallback_v, dst_v, dst->n * sizeof(int));
            } else {
                dst_data->value = result;
            }
        }
    } else {
        // Other is vector format
        const int *other_v = (const int*)other_data;
        
        if (!dst_data->overflow) {
            // Decode our value first
            int dst_v[MAX_PRIMES];
            memset(dst_v, 0, sizeof(dst_v));
            unsigned long long temp = dst_data->value;
            for (int i = 0; i < dst->n; i++) {
                while (temp % PRIMES[i] == 0) {
                    dst_v[i]++;
                    temp /= PRIMES[i];
                }
            }
            
            // Switch to fallback mode
            dst_data->overflow = 1;
            memcpy(dst_data->fallback_v, dst_v, dst->n * sizeof(int));
        }
        
        // Merge with fallback vector
        for (int i = 0; i < dst->n; i++) {
            if (other_v[i] > dst_data->fallback_v[i]) {
                dst_data->fallback_v[i] = other_v[i];
            }
        }
    }
}

TSOrder encoded_compare(const Timestamp *a, const Timestamp *b) {
    const EncodedClockData *a_data = (const EncodedClockData*)a->data;
    const EncodedClockData *b_data = (const EncodedClockData*)b->data;
    
    int a_v[MAX_PRIMES];
    int b_v[MAX_PRIMES];
    memset(a_v, 0, sizeof(a_v));
    memset(b_v, 0, sizeof(b_v));
    
    // Decode a
    if (a_data->overflow) {
        memcpy(a_v, a_data->fallback_v, a->n * sizeof(int));
    } else {
        unsigned long long temp = a_data->value;
        for (int i = 0; i < a->n; i++) {
            while (temp % PRIMES[i] == 0) {
                a_v[i]++;
                temp /= PRIMES[i];
            }
        }
    }
    
    // Decode b
    if (b_data->overflow) {
        memcpy(b_v, b_data->fallback_v, b->n * sizeof(int));
    } else {
        unsigned long long temp = b_data->value;
        for (int i = 0; i < b->n; i++) {
            while (temp % PRIMES[i] == 0) {
                b_v[i]++;
                temp /= PRIMES[i];
            }
        }
    }
    
    // Compare vectors
    int a_le_b = 1, b_le_a = 1;
    int a_lt_b = 0, b_lt_a = 0;
    
    for (int i = 0; i < a->n; i++) {
        if (a_v[i] > b_v[i]) {
            a_le_b = 0;
            b_lt_a = 1;
        }
        if (b_v[i] > a_v[i]) {
            b_le_a = 0;
            a_lt_b = 1;
        }
    }
    
    if (a_le_b && b_le_a) return TS_EQUAL;
    if (a_le_b && a_lt_b) return TS_BEFORE;
    if (b_le_a && b_lt_a) return TS_AFTER;
    return TS_CONCURRENT;
}

size_t encoded_serialize(const Timestamp *ts, void *buffer, size_t bufsize) {
    const EncodedClockData *data = (const EncodedClockData*)ts->data;
    
    if (data->overflow) {
        // Serialize as vector
        size_t required = ts->n * sizeof(int);
        if (bufsize >= required) {
            memcpy(buffer, data->fallback_v, required);
        }
        return required;
    } else {
        // Serialize as encoded value
        size_t required = sizeof(unsigned long long);
        if (bufsize >= required) {
            memcpy(buffer, &data->value, required);
        }
        return required;
    }
}

void encoded_deserialize(Timestamp *ts, const void *buffer, size_t size) {
    EncodedClockData *data = (EncodedClockData*)ts->data;
    
    if (size == sizeof(unsigned long long)) {
        // Encoded format
        data->value = *(const unsigned long long*)buffer;
        data->overflow = 0;
    } else if (size == ts->n * sizeof(int)) {
        // Vector format
        data->overflow = 1;
        memcpy(data->fallback_v, buffer, size);
    }
}

void encoded_to_string(const Timestamp *ts, char *buf, size_t bufsize) {
    const EncodedClockData *data = (const EncodedClockData*)ts->data;
    
    if (data->overflow) {
        size_t used = snprintf(buf, bufsize, "E_OVERFLOW[");
        for (int i = 0; i < ts->n; i++) {
            used += snprintf(buf + used, bufsize - used, "%s%d", 
                            (i ? "," : ""), data->fallback_v[i]);
            if (used >= bufsize) break;
        }
        snprintf(buf + used, bufsize - used, "]");
    } else {
        snprintf(buf, bufsize, "E:%llu", data->value);
    }
}

Timestamp encoded_clone(const Timestamp *ts) {
    Timestamp out = encoded_create(ts->n, ts->pid, ts->type);
    const EncodedClockData *src_data = (const EncodedClockData*)ts->data;
    EncodedClockData *dst_data = (EncodedClockData*)out.data;
    
    dst_data->value = src_data->value;
    dst_data->overflow = src_data->overflow;
    memcpy(dst_data->fallback_v, src_data->fallback_v, ts->n * sizeof(int));
    
    return out;
}

/* ---------- Operations Table ---------- */

TimestampOps ENCODED_OPS = {
    .create = encoded_create,
    .destroy = encoded_destroy,
    .increment = encoded_increment,
    .merge = encoded_merge,
    .compare = encoded_compare,
    .serialize = encoded_serialize,
    .deserialize = encoded_deserialize,
    .to_string = encoded_to_string,
    .clone = encoded_clone
};