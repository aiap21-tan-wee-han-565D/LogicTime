#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "differential_clock.h"

/* ---------- Test Framework ---------- */

typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    char current_test[256];
} TestStats;

static TestStats g_stats = {0};

#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        printf("FAIL: %s - %s\n", g_stats.current_test, message); \
        g_stats.tests_failed++; \
        return 0; \
    } \
} while(0)

#define TEST_ASSERT_EQ(expected, actual, message) do { \
    if ((expected) != (actual)) { \
        printf("FAIL: %s - %s (expected: %d, actual: %d)\n", \
               g_stats.current_test, message, (int)(expected), (int)(actual)); \
        g_stats.tests_failed++; \
        return 0; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    snprintf(g_stats.current_test, sizeof(g_stats.current_test), #test_func); \
    g_stats.tests_run++; \
    if (test_func()) { \
        printf("PASS: %s\n", #test_func); \
        g_stats.tests_passed++; \
    } \
} while(0)

/* ---------- Helper Functions ---------- */

/* ---------- Data Structure Tests ---------- */

static int test_differential_create() {
    Timestamp ts = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    
    TEST_ASSERT(ts.n == 3, "Process count should be 3");
    TEST_ASSERT(ts.pid == 1, "Process ID should be 1");
    TEST_ASSERT(ts.type == CLOCK_DIFFERENTIAL, "Clock type should be DIFFERENTIAL");
    TEST_ASSERT(ts.data != NULL, "Data should not be NULL");
    
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    TEST_ASSERT(data->v != NULL, "Vector array should not be NULL");
    TEST_ASSERT(data->LS != NULL, "LS array should not be NULL");
    TEST_ASSERT(data->LU != NULL, "LU array should not be NULL");
    TEST_ASSERT(data->local_clock == 0, "Local clock should be initialized to 0");
    
    // Check initial values
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ(0, data->v[i], "Vector should be initialized to zeros");
        TEST_ASSERT_EQ(0, data->LS[i], "LS should be initialized to zeros");
        TEST_ASSERT_EQ(0, data->LU[i], "LU should be initialized to zeros");
    }
    
    differential_destroy(&ts);
    return 1;
}

static int test_differential_destroy() {
    Timestamp ts = differential_create(3, 0, CLOCK_DIFFERENTIAL);
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    
    // Verify data exists before destruction
    TEST_ASSERT(data != NULL, "Data should exist before destruction");
    TEST_ASSERT(data->v != NULL, "Vector should exist before destruction");
    
    differential_destroy(&ts);
    
    // After destruction, data should be NULL
    TEST_ASSERT(ts.data == NULL, "Data should be NULL after destruction");
    
    return 1;
}

static int test_differential_clone() {
    Timestamp original = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    
    // Modify original to have some state
    differential_increment(&original);
    differential_increment(&original);
    
    DifferentialClockData *orig_data = (DifferentialClockData*)original.data;
    orig_data->LS[0] = 5;
    orig_data->LU[2] = 3;
    
    Timestamp clone = differential_clone(&original);
    
    TEST_ASSERT(clone.n == original.n, "Clone should have same process count");
    TEST_ASSERT(clone.pid == original.pid, "Clone should have same process ID");
    TEST_ASSERT(clone.type == original.type, "Clone should have same clock type");
    TEST_ASSERT(clone.data != original.data, "Clone data should be different pointer");
    
    DifferentialClockData *clone_data = (DifferentialClockData*)clone.data;
    TEST_ASSERT(clone_data->v != orig_data->v, "Clone vector should be different pointer");
    TEST_ASSERT(clone_data->LS != orig_data->LS, "Clone LS should be different pointer");
    TEST_ASSERT(clone_data->LU != orig_data->LU, "Clone LU should be different pointer");
    
    // Check values are copied correctly
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ(orig_data->v[i], clone_data->v[i], "Vector values should match");
        TEST_ASSERT_EQ(orig_data->LS[i], clone_data->LS[i], "LS values should match");
        TEST_ASSERT_EQ(orig_data->LU[i], clone_data->LU[i], "LU values should match");
    }
    TEST_ASSERT_EQ(orig_data->local_clock, clone_data->local_clock, "Local clock should match");
    
    differential_destroy(&original);
    differential_destroy(&clone);
    return 1;
}

/* ---------- Basic Operations Tests ---------- */

static int test_differential_increment() {
    Timestamp ts = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    
    // Initial state
    TEST_ASSERT_EQ(0, data->v[1], "Initial v[1] should be 0");
    TEST_ASSERT_EQ(0, data->local_clock, "Initial local_clock should be 0");
    TEST_ASSERT_EQ(0, data->LU[1], "Initial LU[1] should be 0");
    
    // First increment
    differential_increment(&ts);
    TEST_ASSERT_EQ(1, data->v[1], "After increment, v[1] should be 1");
    TEST_ASSERT_EQ(1, data->local_clock, "After increment, local_clock should be 1");
    TEST_ASSERT_EQ(1, data->LU[1], "After increment, LU[1] should be 1");
    
    // Second increment
    differential_increment(&ts);
    TEST_ASSERT_EQ(2, data->v[1], "After second increment, v[1] should be 2");
    TEST_ASSERT_EQ(2, data->local_clock, "After second increment, local_clock should be 2");
    TEST_ASSERT_EQ(2, data->LU[1], "After second increment, LU[1] should be 2");
    
    // Other processes should remain unchanged
    TEST_ASSERT_EQ(0, data->v[0], "v[0] should remain 0");
    TEST_ASSERT_EQ(0, data->v[2], "v[2] should remain 0");
    
    differential_destroy(&ts);
    return 1;
}

static int test_differential_to_string() {
    Timestamp ts = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    char buffer[256];
    
    // Test initial state
    differential_to_string(&ts, buffer, sizeof(buffer));
    TEST_ASSERT(strcmp(buffer, "D[0,0,0]") == 0, "Initial string should be D[0,0,0]");
    
    // Test after increment
    differential_increment(&ts);
    differential_to_string(&ts, buffer, sizeof(buffer));
    TEST_ASSERT(strcmp(buffer, "D[0,1,0]") == 0, "After increment should be D[0,1,0]");
    
    // Test with modified vector
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    data->v[0] = 5;
    data->v[2] = 3;
    differential_to_string(&ts, buffer, sizeof(buffer));
    TEST_ASSERT(strcmp(buffer, "D[5,1,3]") == 0, "Modified vector should be D[5,1,3]");
    
    differential_destroy(&ts);
    return 1;
}

/* ---------- Merge Operation Tests ---------- */

static int test_differential_merge_full_vector() {
    Timestamp ts = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    
    // Set up initial state
    data->v[1] = 2;
    data->local_clock = 2;
    data->LU[1] = 2;
    
    // Create other vector to merge
    int other_vector[] = {5, 1, 3};
    
    differential_merge(&ts, other_vector, 3 * sizeof(int));
    
    // After merge: local clock should increment, then merge
    TEST_ASSERT_EQ(3, data->local_clock, "Local clock should increment to 3");
    TEST_ASSERT_EQ(3, data->v[1], "v[1] should increment to 3");
    TEST_ASSERT_EQ(5, data->v[0], "v[0] should be updated to 5");
    TEST_ASSERT_EQ(3, data->v[2], "v[2] should be updated to 3");
    
    // LU should be updated for changed components
    TEST_ASSERT_EQ(3, data->LU[0], "LU[0] should be updated to local_clock");
    TEST_ASSERT_EQ(3, data->LU[1], "LU[1] should be updated to local_clock");
    TEST_ASSERT_EQ(3, data->LU[2], "LU[2] should be updated to local_clock");
    
    differential_destroy(&ts);
    return 1;
}

static int test_differential_merge_differential_format() {
    Timestamp ts = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    
    // Set up initial state
    data->v[1] = 2;
    data->local_clock = 2;
    data->LU[1] = 2;
    
    // Create differential format: [(process_id, value), ...]
    int differential_data[] = {0, 5, 2, 3}; // P0=5, P2=3
    
    differential_merge(&ts, differential_data, 4 * sizeof(int));
    
    // Check results
    TEST_ASSERT_EQ(3, data->local_clock, "Local clock should increment to 3");
    TEST_ASSERT_EQ(3, data->v[1], "v[1] should increment to 3");
    TEST_ASSERT_EQ(5, data->v[0], "v[0] should be updated to 5");
    TEST_ASSERT_EQ(3, data->v[2], "v[2] should be updated to 3");
    
    differential_destroy(&ts);
    return 1;
}

/* ---------- Serialization Tests ---------- */

static int test_differential_serialize_basic() {
    Timestamp ts = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    
    // Set up state
    data->v[0] = 5;
    data->v[1] = 2;
    data->v[2] = 3;
    
    // Test serialization
    int buffer[3];
    size_t required = differential_serialize(&ts, buffer, sizeof(buffer));
    
    TEST_ASSERT_EQ(3 * sizeof(int), required, "Required size should be 3 ints");
    TEST_ASSERT_EQ(5, buffer[0], "Serialized v[0] should be 5");
    TEST_ASSERT_EQ(2, buffer[1], "Serialized v[1] should be 2");
    TEST_ASSERT_EQ(3, buffer[2], "Serialized v[2] should be 3");
    
    differential_destroy(&ts);
    return 1;
}

static int test_differential_serialize_for_dest() {
    Timestamp ts = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    
    // Set up state
    data->v[0] = 5;
    data->v[1] = 2;
    data->v[2] = 3;
    data->local_clock = 2;
    data->LU[0] = 1;
    data->LU[1] = 2;
    data->LU[2] = 1;
    data->LS[0] = 0; // Last sent to process 0 at time 0
    
    // Test destination-aware serialization to process 0
    int buffer[6]; // Enough for 3 pairs
    size_t required = differential_serialize_for_dest(&ts, 0, buffer, sizeof(buffer));
    
    // Should send: (0,5), (1,2), (2,3) because LS[0]=0 < LU[all] and process 1 is always included
    TEST_ASSERT_EQ(6 * sizeof(int), required, "Should send 3 pairs (6 ints)");
    
    // Check LS update
    TEST_ASSERT_EQ(2, data->LS[0], "LS[0] should be updated to local_clock");
    
    differential_destroy(&ts);
    return 1;
}

/* ---------- Comparison Tests ---------- */

static int test_differential_compare() {
    Timestamp ts1 = differential_create(3, 0, CLOCK_DIFFERENTIAL);
    Timestamp ts2 = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    
    DifferentialClockData *data1 = (DifferentialClockData*)ts1.data;
    DifferentialClockData *data2 = (DifferentialClockData*)ts2.data;
    
    // Test EQUAL
    TSOrder result = differential_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_EQUAL, result, "Empty clocks should be equal");
    
    // Test BEFORE: ts1 = [1,0,0], ts2 = [1,1,0]
    data1->v[0] = 1;
    data2->v[0] = 1;
    data2->v[1] = 1;
    result = differential_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_BEFORE, result, "ts1 should be before ts2");
    
    // Test AFTER: ts1 = [2,1,0], ts2 = [1,1,0]
    data1->v[0] = 2;
    data1->v[1] = 1;
    result = differential_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_AFTER, result, "ts1 should be after ts2");
    
    // Test CONCURRENT: ts1 = [2,0,0], ts2 = [0,2,0]
    data1->v[0] = 2;
    data1->v[1] = 0;
    data2->v[0] = 0;
    data2->v[1] = 2;
    result = differential_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_CONCURRENT, result, "ts1 and ts2 should be concurrent");
    
    differential_destroy(&ts1);
    differential_destroy(&ts2);
    return 1;
}

/* ---------- Algorithm-Specific Tests ---------- */

static int test_singhal_kshemkalyani_algorithm() {
    // Test complete send-receive cycle
    Timestamp sender = differential_create(3, 0, CLOCK_DIFFERENTIAL);
    Timestamp receiver = differential_create(3, 1, CLOCK_DIFFERENTIAL);
    
    // Set up sender state
    differential_increment(&sender); // [1,0,0]
    differential_increment(&sender); // [2,0,0]
    
    // Simulate send: increment and serialize for destination
    differential_increment(&sender); // [3,0,0]
    
    int buffer[6];
    size_t msg_size = differential_serialize_for_dest(&sender, 1, buffer, sizeof(buffer));
    
    // Simulate receive: merge and increment
    differential_merge(&receiver, buffer, msg_size);
    
    DifferentialClockData *recv_data = (DifferentialClockData*)receiver.data;
    
    // Receiver should have: incremented own clock, then merged
    TEST_ASSERT_EQ(1, recv_data->v[1], "Receiver should increment own clock");
    TEST_ASSERT_EQ(3, recv_data->v[0], "Receiver should have sender's clock value");
    
    differential_destroy(&sender);
    differential_destroy(&receiver);
    return 1;
}

/* ---------- Edge Case Tests ---------- */

static int test_differential_edge_cases() {
    Timestamp ts = differential_create(1, 0, CLOCK_DIFFERENTIAL);
    
    // Test single process
    differential_increment(&ts);
    DifferentialClockData *data = (DifferentialClockData*)ts.data;
    TEST_ASSERT_EQ(1, data->v[0], "Single process increment should work");
    
    // Test empty merge
    int empty_buffer[2] = {0};
    differential_merge(&ts, empty_buffer, 0);
    TEST_ASSERT_EQ(2, data->v[0], "Merge should still increment local clock");
    
    differential_destroy(&ts);
    return 1;
}

/* ---------- Test Runner ---------- */

static void print_test_summary() {
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", g_stats.tests_run);
    printf("Tests passed: %d\n", g_stats.tests_passed);
    printf("Tests failed: %d\n", g_stats.tests_failed);
    printf("Success rate: %.1f%%\n", 
           g_stats.tests_run > 0 ? (100.0 * g_stats.tests_passed / g_stats.tests_run) : 0.0);
}

int main() {
    printf("=== Differential Clock Test Suite ===\n\n");
    
    // Data Structure Tests
    printf("--- Data Structure Tests ---\n");
    RUN_TEST(test_differential_create);
    RUN_TEST(test_differential_destroy);
    RUN_TEST(test_differential_clone);
    
    // Basic Operations Tests
    printf("\n--- Basic Operations Tests ---\n");
    RUN_TEST(test_differential_increment);
    RUN_TEST(test_differential_to_string);
    
    // Merge Operation Tests
    printf("\n--- Merge Operation Tests ---\n");
    RUN_TEST(test_differential_merge_full_vector);
    RUN_TEST(test_differential_merge_differential_format);
    
    // Serialization Tests
    printf("\n--- Serialization Tests ---\n");
    RUN_TEST(test_differential_serialize_basic);
    RUN_TEST(test_differential_serialize_for_dest);
    
    // Comparison Tests
    printf("\n--- Comparison Tests ---\n");
    RUN_TEST(test_differential_compare);
    
    // Algorithm-Specific Tests
    printf("\n--- Algorithm-Specific Tests ---\n");
    RUN_TEST(test_singhal_kshemkalyani_algorithm);
    
    // Edge Case Tests
    printf("\n--- Edge Case Tests ---\n");
    RUN_TEST(test_differential_edge_cases);
    
    print_test_summary();
    
    return g_stats.tests_failed > 0 ? 1 : 0;
}