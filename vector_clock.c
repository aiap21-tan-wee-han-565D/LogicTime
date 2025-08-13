// vector_clock.c
// Compile: gcc -O2 -pthread vector_clock.c -o vector_clock
// Run:     ./vector_clock [num_processes] [steps_per_process]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    int n;      // number of processes
    int pid;    // this process's ID [0..n-1]
    int *v;     // vector clock
} VectorClock;

typedef enum {
    VC_BEFORE,
    VC_AFTER,
    VC_CONCURRENT,
    VC_EQUAL
} VCOrder;

/* ---------- Vector Clock API ---------- */

VectorClock vc_create(int n, int pid) {
    VectorClock vc;
    vc.n = n;
    vc.pid = pid;
    vc.v = (int*)calloc(n, sizeof(int));
    if (!vc.v) {
        fprintf(stderr, "OOM\n");
        exit(1);
    }
    return vc;
}

void vc_destroy(VectorClock *vc) {
    if (vc && vc->v) { free(vc->v); vc->v = NULL; }
}

void vc_copy_into(int *dst, const int *src, int n) {
    memcpy(dst, src, n * sizeof(int));
}

VectorClock vc_clone(const VectorClock *vc) {
    VectorClock out = vc_create(vc->n, vc->pid);
    vc_copy_into(out.v, vc->v, vc->n);
    return out;
}

void vc_increment(VectorClock *vc) {
    vc->v[vc->pid] += 1;
}

void vc_merge(VectorClock *dst, const int *other) {
    for (int i = 0; i < dst->n; i++) {
        if (other[i] > dst->v[i]) dst->v[i] = other[i];
    }
}

VCOrder vc_compare_raw(const int *a, const int *b, int n) {
    int a_le_b = 1, b_le_a = 1;
    int a_lt_b = 0, b_lt_a = 0;
    for (int i = 0; i < n; i++) {
        if (a[i] > b[i]) a_le_b = 0, b_lt_a = 1;
        if (b[i] > a[i]) b_le_a = 0, a_lt_b = 1;
    }
    if (a_le_b && b_le_a) return VC_EQUAL;
    if (a_le_b && a_lt_b) return VC_BEFORE;
    if (b_le_a && b_lt_a) return VC_AFTER;
    return VC_CONCURRENT;
}

VCOrder vc_compare(const VectorClock *A, const VectorClock *B) {
    if (A->n != B->n) {
        fprintf(stderr, "Mismatched vector sizes!\n");
        exit(1);
    }
    return vc_compare_raw(A->v, B->v, A->n);
}

void vc_to_string_buf(const int *v, int n, char *buf, size_t bufsz) {
    size_t used = 0;
    used += snprintf(buf + used, bufsz - used, "[");
    for (int i = 0; i < n; i++) {
        used += snprintf(buf + used, bufsz - used, "%s%d", (i ? "," : ""), v[i]);
        if (used >= bufsz) break;
    }
    snprintf(buf + used, bufsz - used, "]");
}

/* ---------- Simple message queue (per process) ---------- */

typedef struct Message {
    int from;
    int to;
    int *vc;                // snapshot of sender's clock
    char payload[64];
    struct Message *next;
} Message;

typedef struct {
    Message *head, *tail;
    int size;
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
} MsgQueue;

void mq_init(MsgQueue *q) {
    q->head = q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->cv, NULL);
}

void mq_destroy(MsgQueue *q) {
    pthread_mutex_lock(&q->mtx);
    Message *cur = q->head;
    while (cur) {
        Message *nxt = cur->next;
        free(cur->vc);
        free(cur);
        cur = nxt;
    }
    q->head = q->tail = NULL;
    q->size = 0;
    pthread_mutex_unlock(&q->mtx);
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->cv);
}

void mq_push(MsgQueue *q, Message *m) {
    m->next = NULL;
    pthread_mutex_lock(&q->mtx);
    if (!q->tail) q->head = q->tail = m;
    else { q->tail->next = m; q->tail = m; }
    q->size++;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mtx);
}

// Non-blocking pop; returns NULL if empty
Message* mq_try_pop(MsgQueue *q) {
    pthread_mutex_lock(&q->mtx);
    Message *m = q->head;
    if (m) {
        q->head = m->next;
        if (!q->head) q->tail = NULL;
        q->size--;
    }
    pthread_mutex_unlock(&q->mtx);
    return m;
}

/* ---------- “Distributed system” worker threads ---------- */

typedef struct {
    int pid;
    int n;
    int steps;
    VectorClock vc;
    MsgQueue *queues;   // array of size n (one per process)
} ProcCtx;

static inline void ms_sleep(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void print_event_header(int pid, const int *v, int n, const char *etype) {
    char buf[256];
    vc_to_string_buf(v, n, buf, sizeof(buf));
    printf("P%d %s | VC=%s | ", pid, etype, buf);
}

static void do_internal(ProcCtx *ctx) {
    vc_increment(&ctx->vc);
    print_event_header(ctx->pid, ctx->vc.v, ctx->n, "INTERNAL");
    printf("local computation\n");
}

static void do_send(ProcCtx *ctx, int dest, const char *payload) {
    if (dest == ctx->pid) return; // shouldn't happen
    vc_increment(&ctx->vc); // send event
    // Prepare message
    Message *m = (Message*)malloc(sizeof(Message));
    m->from = ctx->pid;
    m->to = dest;
    m->vc = (int*)malloc(ctx->n * sizeof(int));
    vc_copy_into(m->vc, ctx->vc.v, ctx->n);
    snprintf(m->payload, sizeof(m->payload), "%s", payload);
    mq_push(&ctx->queues[dest], m);
    print_event_header(ctx->pid, ctx->vc.v, ctx->n, "SEND    ");
    printf("to P%d, payload=\"%s\"\n", dest, m->payload);
}

static int do_try_recv(ProcCtx *ctx) {
    Message *m = mq_try_pop(&ctx->queues[ctx->pid]);
    if (!m) return 0;

    // Merge, then increment own entry for the receive event
    print_event_header(ctx->pid, ctx->vc.v, ctx->n, "RECV(BEFORE)");
    printf("from P%d: payload=\"%s\", msgVC=", m->from, m->payload);
    char buf[256];
    vc_to_string_buf(m->vc, ctx->n, buf, sizeof(buf));
    printf("%s\n", buf);

    vc_merge(&ctx->vc, m->vc);
    vc_increment(&ctx->vc);

    print_event_header(ctx->pid, ctx->vc.v, ctx->n, "RECV(AFTER) ");
    printf("merged with sender and incremented\n");

    free(m->vc);
    free(m);
    return 1;
}

static int rand_in_range(unsigned int *seed, int lo, int hi_inclusive) {
    // rand_r returns 0..RAND_MAX
    int r = rand_r(seed);
    return lo + (r % (hi_inclusive - lo + 1));
}

static void* worker(void *arg) {
    ProcCtx *ctx = (ProcCtx*)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (ctx->pid * 2654435761u);

    for (int step = 0; step < ctx->steps; step++) {
        int choice = rand_in_range(&seed, 0, 99);

        if (choice < 35) {
            do_internal(ctx);
        } else if (choice < 75) {
            // SEND
            int dest;
            do { dest = rand_in_range(&seed, 0, ctx->n - 1); } while (dest == ctx->pid);
            char payload[64];
            snprintf(payload, sizeof(payload), "hello_%d_from_P%d", step, ctx->pid);
            do_send(ctx, dest, payload);
        } else {
            // TRY RECEIVE; if nothing, do internal
            if (!do_try_recv(ctx)) {
                do_internal(ctx);
            }
        }

        // Short stochastic delay to interleave events
        ms_sleep(rand_in_range(&seed, 5, 25));
    }

    // Drain a few possible remaining messages (non-blocking)
    for (int i = 0; i < 4; i++) {
        if (!do_try_recv(ctx)) break;
        ms_sleep(3);
    }

    return NULL;
}

/* ---------- Demo driver ---------- */

int main(int argc, char **argv) {
    int n = 3;
    int steps = 12;
    if (argc >= 2) n = atoi(argv[1]);
    if (argc >= 3) steps = atoi(argv[2]);
    if (n < 2) { fprintf(stderr, "Use at least 2 processes.\n"); return 1; }

    MsgQueue *queues = (MsgQueue*)malloc(n * sizeof(MsgQueue));
    for (int i = 0; i < n; i++) mq_init(&queues[i]);

    ProcCtx *procs = (ProcCtx*)malloc(n * sizeof(ProcCtx));
    pthread_t *threads = (pthread_t*)malloc(n * sizeof(pthread_t));

    for (int i = 0; i < n; i++) {
        procs[i].pid = i;
        procs[i].n = n;
        procs[i].steps = steps;
        procs[i].vc = vc_create(n, i);
        procs[i].queues = queues;
    }

    printf("=== Vector Clock Demo: %d processes, %d steps each ===\n", n, steps);
    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, worker, &procs[i]);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    // Show pairwise comparisons of final clocks
    printf("\n=== Final vector clocks ===\n");
    for (int i = 0; i < n; i++) {
        char buf[256];
        vc_to_string_buf(procs[i].vc.v, n, buf, sizeof(buf));
        printf("P%d: %s\n", i, buf);
    }

    printf("\n=== Pairwise partial order (A ? B) ===\n");
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            VCOrder o = vc_compare(&procs[i].vc, &procs[j].vc);
            const char *rel = (o == VC_BEFORE) ? "BEFORE"
                               : (o == VC_AFTER) ? "AFTER"
                               : (o == VC_EQUAL) ? "EQUAL"
                               : "CONCURRENT";
            printf("P%d vs P%d: %s\n", i, j, rel);
        }
    }

    // Cleanup
    for (int i = 0; i < n; i++) {
        vc_destroy(&procs[i].vc);
    }
    for (int i = 0; i < n; i++) mq_destroy(&queues[i]);
    free(queues);
    free(procs);
    free(threads);

    return 0;
}
