#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stddef.h>

/* ---------- Clock Type Configuration ---------- */

typedef enum {
    CLOCK_STANDARD = 0,   // Original full vector clocks
    CLOCK_SPARSE = 1,     // Compressed/sparse representation
    CLOCK_DIFFERENTIAL = 2, // Singhal-Kshemkalyani technique
    CLOCK_ENCODED = 3     // Prime number encoding
} ClockType;

typedef enum {
    TS_BEFORE,
    TS_AFTER,
    TS_CONCURRENT,
    TS_EQUAL
} TSOrder;

/* ---------- Generic Timestamp Structure ---------- */

typedef struct {
    int n;              // number of processes
    int pid;            // this process's ID [0..n-1]
    ClockType type;     // clock implementation type
    void *data;         // clock-specific data
    size_t data_size;   // size of serialized data
} Timestamp;

/* ---------- Abstract Timestamp Operations ---------- */

typedef struct {
    Timestamp (*create)(int n, int pid, ClockType type);
    void (*destroy)(Timestamp *ts);
    void (*increment)(Timestamp *ts);
    void (*merge)(Timestamp *dst, const void *other_data, size_t other_size);
    TSOrder (*compare)(const Timestamp *a, const Timestamp *b);
    size_t (*serialize)(const Timestamp *ts, void *buffer, size_t bufsize);
    size_t (*serialize_for_dest)(const Timestamp *ts, int dest, void *buffer, size_t bufsize);
    void (*deserialize)(Timestamp *ts, const void *buffer, size_t size);
    void (*to_string)(const Timestamp *ts, char *buf, size_t bufsize);
    Timestamp (*clone)(const Timestamp *ts);
} TimestampOps;

/* ---------- Main Timestamp Interface ---------- */

Timestamp ts_create(int n, int pid, ClockType type);
void ts_destroy(Timestamp *ts);
void ts_increment(Timestamp *ts);
void ts_merge(Timestamp *dst, const void *other_data, size_t other_size);
TSOrder ts_compare(const Timestamp *a, const Timestamp *b);
size_t ts_serialize(const Timestamp *ts, void *buffer, size_t bufsize);
size_t ts_serialize_for_dest(const Timestamp *ts, int dest, void *buffer, size_t bufsize);
void ts_deserialize(Timestamp *ts, const void *buffer, size_t size);
void ts_to_string(const Timestamp *ts, char *buf, size_t bufsize);
Timestamp ts_clone(const Timestamp *ts);

/* ---------- Clock Type Information ---------- */

extern const char* clock_type_names[];
extern const char* clock_type_descriptions[];

#endif // TIMESTAMP_H