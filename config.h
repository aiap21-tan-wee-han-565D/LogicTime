#ifndef CONFIG_H
#define CONFIG_H

/* ---------- Configuration Constants ---------- */

// Default simulation parameters
#define DEFAULT_PROCESSES 3
#define DEFAULT_STEPS 12

// Event probability distribution (out of 100)
#define PROB_INTERNAL 35    // 35% probability for internal events
#define PROB_SEND 40        // 40% probability for send events (35-75)
#define PROB_RECV 25        // 25% probability for receive events (75-100)

// Timing parameters
#define MIN_SLEEP_MS 5      // Minimum sleep between events
#define MAX_SLEEP_MS 25     // Maximum sleep between events
#define DRAIN_ATTEMPTS 4    // Attempts to drain messages at end

// Buffer sizes
#define PAYLOAD_SIZE 64
#define STRING_BUFFER_SIZE 256

/* ---------- Compile-time Validation ---------- */

#if (PROB_INTERNAL + PROB_SEND + PROB_RECV) != 100
#error "Event probabilities must sum to 100"
#endif

#endif // CONFIG_H