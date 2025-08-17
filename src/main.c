#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "timestamp.h"
#include "message_queue.h"
#include "simulation.h"
#include "config.h"

/* ---------- Help and Usage ---------- */

void print_usage(const char* prog_name) {
    printf("Usage: %s [num_processes] [steps_per_process] [clock_type]\n\n", prog_name);
    printf("Parameters:\n");
    printf("  num_processes     : Number of simulated processes (default: %d, min: 2)\n", DEFAULT_PROCESSES);
    printf("  steps_per_process : Number of steps per process (default: %d)\n", DEFAULT_STEPS);
    printf("  clock_type       : Clock implementation type (default: 0)\n\n");
    printf("Clock Types:\n");
    for (int i = 0; i < 5; i++) {
        printf("  %d - %s: %s\n", i, clock_type_names[i], clock_type_descriptions[i]);
    }
    printf("\nExample: %s 5 20 1    # 5 processes, 20 steps each, sparse clocks\n", prog_name);
}

/* ---------- Performance Display ---------- */

void display_performance_stats(int n, ClockType clock_type) {
    // Display performance statistics
    printf("\n=== Performance Statistics ===\n");
    printf("Total messages sent: %d\n", perf_stats.total_messages);
    printf("Total timestamp bytes: %zu bytes\n", perf_stats.total_message_bytes);
    if (perf_stats.total_messages > 0) {
        printf("Average timestamp size: %.2f bytes\n", perf_stats.avg_clock_size);
        printf("Max timestamp size: %d bytes\n", perf_stats.max_clock_size);
        printf("Avg bytes per message: %.2f bytes\n", 
               (double)perf_stats.total_message_bytes / perf_stats.total_messages);
    }
    
    // Calculate baseline comparison (standard vector clock for same n)
    size_t standard_size = n * sizeof(int);
    printf("\nComparison to Standard Vector Clock:\n");
    printf("Standard timestamp size: %zu bytes\n", standard_size);
    if (perf_stats.total_messages > 0) {
        double compression_ratio = perf_stats.avg_clock_size / (double)standard_size;
        printf("Compression ratio: %.2fx %s\n", 
               compression_ratio < 1.0 ? 1.0/compression_ratio : compression_ratio,
               compression_ratio < 1.0 ? "(smaller)" : "(larger)");
    }
}

/* ---------- Main Demo Driver ---------- */

int main(int argc, char **argv) {
    int n = DEFAULT_PROCESSES;
    int steps = DEFAULT_STEPS;
    ClockType clock_type = CLOCK_STANDARD;
    
    // Handle help request
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (argc >= 2) n = atoi(argv[1]);
    if (argc >= 3) steps = atoi(argv[2]);
    if (argc >= 4) {
        clock_type = (ClockType)atoi(argv[3]);
        if (clock_type < 0 || clock_type > 4) {
            fprintf(stderr, "Invalid clock type. Use 0-4.\n");
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (n < 2) { 
        fprintf(stderr, "Use at least 2 processes.\n"); 
        print_usage(argv[0]);
        return 1; 
    }

    MsgQueue *queues = (MsgQueue*)malloc(n * sizeof(MsgQueue));
    for (int i = 0; i < n; i++) mq_init(&queues[i]);

    ProcCtx *procs = (ProcCtx*)malloc(n * sizeof(ProcCtx));
    pthread_t *threads = (pthread_t*)malloc(n * sizeof(pthread_t));

    for (int i = 0; i < n; i++) {
        procs[i].pid = i;
        procs[i].n = n;
        procs[i].steps = steps;
        procs[i].current_step = 0;  // Initialize current step
        procs[i].clock_type = clock_type;
        procs[i].ts = ts_create(n, i, clock_type);
        procs[i].queues = queues;
    }

    printf("=== %s Clock Demo ===\n", clock_type_names[clock_type]);
    printf("Configuration: %d processes, %d steps each\n", n, steps);
    printf("Description: %s\n\n", clock_type_descriptions[clock_type]);
    
    // Reset performance stats
    memset(&perf_stats, 0, sizeof(perf_stats));

    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, worker, &procs[i]);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    // Show pairwise comparisons of final clocks
    printf("\n=== Final %s clocks ===\n", clock_type_names[clock_type]);
    for (int i = 0; i < n; i++) {
        char buf[256];
        ts_to_string(&procs[i].ts, buf, sizeof(buf));
        printf("P%d: %s\n", i, buf);
    }

    printf("\n=== Pairwise partial order (A ? B) ===\n");
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            TSOrder o = ts_compare(&procs[i].ts, &procs[j].ts);
            const char *rel = (o == TS_BEFORE) ? "BEFORE"
                               : (o == TS_AFTER) ? "AFTER"
                               : (o == TS_EQUAL) ? "EQUAL"
                               : "CONCURRENT";
            printf("P%d vs P%d: %s\n", i, j, rel);
        }
    }
    
    display_performance_stats(n, clock_type);

    // Cleanup
    for (int i = 0; i < n; i++) {
        ts_destroy(&procs[i].ts);
    }
    for (int i = 0; i < n; i++) mq_destroy(&queues[i]);
    free(queues);
    free(procs);
    free(threads);

    return 0;
}