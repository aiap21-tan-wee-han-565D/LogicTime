#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "simulation.h"
#include "config.h"

/* ---------- Performance Statistics ---------- */

PerfStats perf_stats = {0};

void update_perf_stats(size_t message_size, size_t clock_size) {
    perf_stats.total_message_bytes += message_size;
    perf_stats.total_messages++;
    if (clock_size > perf_stats.max_clock_size) {
        perf_stats.max_clock_size = clock_size;
    }
    perf_stats.avg_clock_size = ((perf_stats.avg_clock_size * (perf_stats.total_messages - 1)) + clock_size) / perf_stats.total_messages;
}

/* ---------- Utility Functions ---------- */

void ms_sleep(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int rand_in_range(unsigned int *seed, int lo, int hi_inclusive) {
    // rand_r returns 0..RAND_MAX
    int r = rand_r(seed);
    return lo + (r % (hi_inclusive - lo + 1));
}

void print_event_header(int pid, const Timestamp *ts, const char *etype) {
    char buf[STRING_BUFFER_SIZE];
    ts_to_string(ts, buf, sizeof(buf));
    printf("P%d %s | TS=%s | ", pid, etype, buf);
}

/* ---------- Event Handlers ---------- */

void do_internal(ProcCtx *ctx) {
    ts_increment(&ctx->ts);
    print_event_header(ctx->pid, &ctx->ts, "INTERNAL");
    printf("local computation\n");
}

void do_send(ProcCtx *ctx, int dest, const char *payload) {
    if (dest == ctx->pid) return; // shouldn't happen
    
    // Always increment timestamp for send events (step 1 of SK algorithm)
    ts_increment(&ctx->ts);
    
    // Prepare message
    Message *m = (Message*)malloc(sizeof(Message));
    m->from = ctx->pid;
    m->to = dest;
    m->clock_type = ctx->clock_type;
    
    // Use destination-aware serialization if available (for differential clocks)
    m->timestamp_size = ts_serialize_for_dest(&ctx->ts, dest, NULL, 0); // Get required size
    m->timestamp_data = malloc(m->timestamp_size);
    ts_serialize_for_dest(&ctx->ts, dest, m->timestamp_data, m->timestamp_size);
    
    // Update performance statistics
    update_perf_stats(sizeof(Message) + m->timestamp_size, m->timestamp_size);
    
    snprintf(m->payload, sizeof(m->payload), "%s", payload);
    mq_push(&ctx->queues[dest], m);
    print_event_header(ctx->pid, &ctx->ts, "SEND    ");
    printf("to P%d, payload=\"%s\"\n", dest, m->payload);
}

int do_try_recv(ProcCtx *ctx) {
    Message *m = mq_try_pop(&ctx->queues[ctx->pid]);
    if (!m) return 0;

    // Display the receive event before merging
    print_event_header(ctx->pid, &ctx->ts, "RECV(BEFORE)");
    
    // Create temporary timestamp for message display
    Timestamp msg_ts = ts_create(ctx->n, m->from, m->clock_type);
    ts_deserialize(&msg_ts, m->timestamp_data, m->timestamp_size);
    
    char buf[STRING_BUFFER_SIZE];
    ts_to_string(&msg_ts, buf, sizeof(buf));
    printf("from P%d: payload=\"%s\", msgTS=%s\n", m->from, m->payload, buf);

    // For differential clocks, merge handles the increment internally
    // For other clocks, merge then increment separately
    ts_merge(&ctx->ts, m->timestamp_data, m->timestamp_size);
    if (ctx->clock_type != CLOCK_DIFFERENTIAL) {
        ts_increment(&ctx->ts);
    }

    print_event_header(ctx->pid, &ctx->ts, "RECV(AFTER) ");
    printf("merged with sender and incremented\n");

    ts_destroy(&msg_ts);
    if (m->timestamp_data) {
        free(m->timestamp_data);
    }
    free(m);
    return 1;
}

/* ---------- Worker Thread ---------- */

void* worker(void *arg) {
    ProcCtx *ctx = (ProcCtx*)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (ctx->pid * 2654435761u);

    for (int step = 0; step < ctx->steps; step++) {
        int choice = rand_in_range(&seed, 0, 99);

        if (choice < PROB_INTERNAL) {
            do_internal(ctx);
        } else if (choice < PROB_INTERNAL + PROB_SEND) {
            // SEND
            int dest;
            do { dest = rand_in_range(&seed, 0, ctx->n - 1); } while (dest == ctx->pid);
            char payload[PAYLOAD_SIZE];
            snprintf(payload, sizeof(payload), "step %d_:hello_to_%d_from_P%d", step, dest, ctx->pid);
            do_send(ctx, dest, payload);
        } else {
            // TRY RECEIVE; if nothing, do internal
            if (!do_try_recv(ctx)) {
                do_internal(ctx);
            }
        }

        // Short stochastic delay to interleave events
        ms_sleep(rand_in_range(&seed, MIN_SLEEP_MS, MAX_SLEEP_MS));
    }

    // Drain a few possible remaining messages (non-blocking)
    for (int i = 0; i < DRAIN_ATTEMPTS; i++) {
        if (!do_try_recv(ctx)) break;
        ms_sleep(3);
    }

    return NULL;
}