#ifndef SIMULATION_H
#define SIMULATION_H

#include "timestamp.h"
#include "message_queue.h"

/* ---------- Process Context Structure ---------- */

typedef struct {
    int pid;
    int n;
    int steps;
    int current_step;   // current step number in simulation
    Timestamp ts;       // timestamp using configured clock type
    MsgQueue *queues;   // array of size n (one per process)
    ClockType clock_type; // clock type for this simulation
} ProcCtx;

/* ---------- Performance Statistics ---------- */

typedef struct {
    size_t total_message_bytes;
    int total_messages;
    int max_clock_size;
    double avg_clock_size;
} PerfStats;

extern PerfStats perf_stats;

/* ---------- Simulation Functions ---------- */

void update_perf_stats(size_t message_size, size_t clock_size);
void print_event_header(int pid, int step, const Timestamp *ts, const char *etype);
void* worker(void *arg);

/* ---------- Event Handlers ---------- */

void do_internal(ProcCtx *ctx);
void do_send(ProcCtx *ctx, int dest, const char *payload);
int do_try_recv(ProcCtx *ctx);

/* ---------- Utility Functions ---------- */

void ms_sleep(int ms);
int rand_in_range(unsigned int *seed, int lo, int hi_inclusive);

#endif // SIMULATION_H