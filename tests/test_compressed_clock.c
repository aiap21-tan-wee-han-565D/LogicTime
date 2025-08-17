#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compressed_clock.h"

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

/* ---------- Data Structure Tests ---------- */

static int test_compressed_create() {
    Timestamp ts = compressed_create(3, 1, CLOCK_COMPRESSED);
    
    TEST_ASSERT(ts.n == 3, "Process count should be 3");
    TEST_ASSERT(ts.pid == 1, "Process ID should be 1");
    TEST_ASSERT(ts.type == CLOCK_COMPRESSED, "Clock type should be COMPRESSED");
    TEST_ASSERT(ts.data != NULL, "Data should not be NULL");
    
    CompressedClockData *data = (CompressedClockData*)ts.data;
    TEST_ASSERT(data->vt != NULL, "Vector clock should not be NULL");
    TEST_ASSERT(data->tau != NULL, "Tau matrix should not be NULL");
    TEST_ASSERT(data->n == 3, "Internal n should be 3");
    
    // Check initial values
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ(0, data->vt[i], "Vector should be initialized to zeros");
        for (int j = 0; j < 3; j++) {
            TEST_ASSERT_EQ(0, data->tau[i][j], "Tau matrix should be initialized to zeros");
        }
    }
    
    compressed_destroy(&ts);
    return 1;
}

static int test_compressed_destroy() {
    Timestamp ts = compressed_create(3, 0, CLOCK_COMPRESSED);
    CompressedClockData *data = (CompressedClockData*)ts.data;
    
    // Verify data exists before destruction
    TEST_ASSERT(data != NULL, "Data should exist before destruction");
    TEST_ASSERT(data->vt != NULL, "Vector should exist before destruction");
    TEST_ASSERT(data->tau != NULL, "Tau matrix should exist before destruction");
    
    compressed_destroy(&ts);
    
    // After destruction, data should be NULL
    TEST_ASSERT(ts.data == NULL, "Data should be NULL after destruction");
    
    return 1;
}

static int test_compressed_clone() {
    Timestamp original = compressed_create(3, 1, CLOCK_COMPRESSED);
    
    // Modify original to have some state
    compressed_increment(&original);
    compressed_increment(&original);
    
    CompressedClockData *orig_data = (CompressedClockData*)original.data;
    orig_data->tau[0][1] = 5;  // Simulate having sent to process 0
    orig_data->tau[2][1] = 3;  // Simulate having sent to process 2
    
    Timestamp clone = compressed_clone(&original);
    
    TEST_ASSERT(clone.n == original.n, "Clone should have same process count");
    TEST_ASSERT(clone.pid == original.pid, "Clone should have same process ID");
    TEST_ASSERT(clone.type == original.type, "Clone should have same clock type");
    TEST_ASSERT(clone.data != original.data, "Clone data should be different pointer");
    
    CompressedClockData *clone_data = (CompressedClockData*)clone.data;
    TEST_ASSERT(clone_data->vt != orig_data->vt, "Clone vector should be different pointer");
    TEST_ASSERT(clone_data->tau != orig_data->tau, "Clone tau should be different pointer");
    
    // Check values are copied correctly
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ(orig_data->vt[i], clone_data->vt[i], "Vector values should match");
        for (int j = 0; j < 3; j++) {
            TEST_ASSERT_EQ(orig_data->tau[i][j], clone_data->tau[i][j], "Tau values should match");
        }
    }
    
    compressed_destroy(&original);
    compressed_destroy(&clone);
    return 1;
}

/* ---------- Basic Operations Tests ---------- */

static int test_compressed_increment() {
    Timestamp ts = compressed_create(3, 1, CLOCK_COMPRESSED);
    CompressedClockData *data = (CompressedClockData*)ts.data;
    
    // Initial state
    TEST_ASSERT_EQ(0, data->vt[1], "Initial vt[1] should be 0");
    
    // First increment
    compressed_increment(&ts);
    TEST_ASSERT_EQ(1, data->vt[1], "After increment, vt[1] should be 1");
    
    // Second increment
    compressed_increment(&ts);
    TEST_ASSERT_EQ(2, data->vt[1], "After second increment, vt[1] should be 2");
    
    // Other processes should remain unchanged
    TEST_ASSERT_EQ(0, data->vt[0], "vt[0] should remain 0");
    TEST_ASSERT_EQ(0, data->vt[2], "vt[2] should remain 0");
    
    compressed_destroy(&ts);
    return 1;
}

static int test_compressed_to_string() {
    Timestamp ts = compressed_create(3, 1, CLOCK_COMPRESSED);
    char buffer[256];
    
    // Test initial state
    compressed_to_string(&ts, buffer, sizeof(buffer));
    TEST_ASSERT(strcmp(buffer, "C[0,0,0]") == 0, "Initial string should be C[0,0,0]");
    
    // Test after increment
    compressed_increment(&ts);
    compressed_to_string(&ts, buffer, sizeof(buffer));
    TEST_ASSERT(strcmp(buffer, "C[0,1,0]") == 0, "After increment should be C[0,1,0]");
    
    // Test with modified vector
    CompressedClockData *data = (CompressedClockData*)ts.data;
    data->vt[0] = 5;
    data->vt[2] = 3;
    compressed_to_string(&ts, buffer, sizeof(buffer));
    TEST_ASSERT(strcmp(buffer, "C[5,1,3]") == 0, "Modified vector should be C[5,1,3]");
    
    compressed_destroy(&ts);
    return 1;
}

/* ---------- Compression Algorithm Tests ---------- */

static int test_compressed_serialize_for_dest_basic() {
    Timestamp ts = compressed_create(4, 2, CLOCK_COMPRESSED);
    CompressedClockData *data = (CompressedClockData*)ts.data;
    
    // Set up initial state: vt = [5, 7, 1, 0]
    data->vt[0] = 5;
    data->vt[1] = 7;
    data->vt[2] = 1;  // This process
    data->vt[3] = 0;
    
    // Simulate last sent to process 3: tau[3] = [5, 7, 1, 0] (same as current)
    memcpy(data->tau[3], data->vt, 4 * sizeof(int));
    
    int buffer[10];
    size_t size = compressed_serialize_for_dest(&ts, 3, buffer, sizeof(buffer));
    
    // After increment, vt[2] becomes 2, so only index 2 should be sent
    TEST_ASSERT_EQ(3 * sizeof(int), size, "Should send 3 ints: count + 1 pair");
    TEST_ASSERT_EQ(1, buffer[0], "Should send 1 changed entry");
    TEST_ASSERT_EQ(2, buffer[1], "Changed index should be 2");
    TEST_ASSERT_EQ(2, buffer[2], "New value should be 2");
    
    // Verify tau was updated
    TEST_ASSERT_EQ(2, data->tau[3][2], "tau[3][2] should be updated to 2");
    
    compressed_destroy(&ts);
    return 1;
}

static int test_compressed_serialize_for_dest_multiple_changes() {
    Timestamp ts = compressed_create(4, 2, CLOCK_COMPRESSED);
    CompressedClockData *data = (CompressedClockData*)ts.data;
    
    // Current state: vt = [5, 8, 1, 0]
    data->vt[0] = 5;
    data->vt[1] = 8;
    data->vt[2] = 1;
    data->vt[3] = 0;
    
    // Last sent to process 3: tau[3] = [5, 7, 1, 0] (different at index 1)
    data->tau[3][0] = 5;
    data->tau[3][1] = 7;  // Different!
    data->tau[3][2] = 1;
    data->tau[3][3] = 0;
    
    int buffer[10];
    size_t size = compressed_serialize_for_dest(&ts, 3, buffer, sizeof(buffer));
    
    // Should send: index 1 (7->8) and index 2 (1->2 after increment)
    TEST_ASSERT_EQ(5 * sizeof(int), size, "Should send 5 ints: count + 2 pairs");
    TEST_ASSERT_EQ(2, buffer[0], "Should send 2 changed entries");
    
    // Check the pairs (order might vary, so check both possibilities)
    int found_idx1 = 0, found_idx2 = 0;
    for (int i = 0; i < 2; i++) {
        int idx = buffer[1 + i * 2];
        int val = buffer[1 + i * 2 + 1];
        
        if (idx == 1 && val == 8) found_idx1 = 1;
        if (idx == 2 && val == 2) found_idx2 = 1;
    }
    
    TEST_ASSERT(found_idx1, "Should find (1, 8) pair");
    TEST_ASSERT(found_idx2, "Should find (2, 2) pair");
    
    compressed_destroy(&ts);
    return 1;
}

static int test_compressed_serialize_for_dest_first_send() {
    Timestamp ts = compressed_create(3, 1, CLOCK_COMPRESSED);
    CompressedClockData *data = (CompressedClockData*)ts.data;
    
    // Set up some state
    data->vt[0] = 2;
    data->vt[1] = 3;
    data->vt[2] = 1;
    
    // tau[0] is all zeros (never sent to process 0 before)
    
    int buffer[10];
    size_t size = compressed_serialize_for_dest(&ts, 0, buffer, sizeof(buffer));
    
    // All non-zero entries should be sent (plus own entry after increment)
    TEST_ASSERT_EQ(7 * sizeof(int), size, "Should send 7 ints: count + 3 pairs");
    TEST_ASSERT_EQ(3, buffer[0], "Should send 3 entries");
    
    compressed_destroy(&ts);
    return 1;
}

/* ---------- Merge Tests ---------- */

static int test_compressed_merge_full_vector() {
    Timestamp ts = compressed_create(3, 1, CLOCK_COMPRESSED);
    CompressedClockData *data = (CompressedClockData*)ts.data;
    
    // Set up initial state
    data->vt[1] = 2;
    
    // Create other vector to merge
    int other_vector[] = {5, 1, 3};
    
    compressed_merge(&ts, other_vector, 3 * sizeof(int));
    
    // After merge: own clock should increment, then merge
    TEST_ASSERT_EQ(3, data->vt[1], "vt[1] should increment to 3");
    TEST_ASSERT_EQ(5, data->vt[0], "vt[0] should be updated to 5");
    TEST_ASSERT_EQ(3, data->vt[2], "vt[2] should be updated to 3");
    
    compressed_destroy(&ts);
    return 1;
}

static int test_compressed_merge_compressed_format() {
    Timestamp ts = compressed_create(3, 1, CLOCK_COMPRESSED);
    CompressedClockData *data = (CompressedClockData*)ts.data;
    
    // Set up initial state
    data->vt[1] = 2;
    
    // Create compressed format: [count=2, (0,5), (2,3)]
    int compressed_data[] = {2, 0, 5, 2, 3};
    
    compressed_merge(&ts, compressed_data, 5 * sizeof(int));
    
    // Check results
    TEST_ASSERT_EQ(3, data->vt[1], "vt[1] should increment to 3");
    TEST_ASSERT_EQ(5, data->vt[0], "vt[0] should be updated to 5");
    TEST_ASSERT_EQ(3, data->vt[2], "vt[2] should be updated to 3");
    
    compressed_destroy(&ts);
    return 1;
}

/* ---------- Comparison Tests ---------- */

static int test_compressed_compare() {
    Timestamp ts1 = compressed_create(3, 0, CLOCK_COMPRESSED);
    Timestamp ts2 = compressed_create(3, 1, CLOCK_COMPRESSED);
    
    CompressedClockData *data1 = (CompressedClockData*)ts1.data;
    CompressedClockData *data2 = (CompressedClockData*)ts2.data;
    
    // Test EQUAL
    TSOrder result = compressed_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_EQUAL, result, "Empty clocks should be equal");
    
    // Test BEFORE: ts1 = [1,0,0], ts2 = [1,1,0]
    data1->vt[0] = 1;
    data2->vt[0] = 1;
    data2->vt[1] = 1;
    result = compressed_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_BEFORE, result, "ts1 should be before ts2");
    
    // Test AFTER: ts1 = [2,1,0], ts2 = [1,1,0]
    data1->vt[0] = 2;
    data1->vt[1] = 1;
    result = compressed_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_AFTER, result, "ts1 should be after ts2");
    
    // Test CONCURRENT: ts1 = [2,0,0], ts2 = [0,2,0]
    data1->vt[0] = 2;
    data1->vt[1] = 0;
    data2->vt[0] = 0;
    data2->vt[1] = 2;
    result = compressed_compare(&ts1, &ts2);
    TEST_ASSERT_EQ(TS_CONCURRENT, result, "ts1 and ts2 should be concurrent");
    
    compressed_destroy(&ts1);
    compressed_destroy(&ts2);
    return 1;
}

/* ---------- Integration Test ---------- */

static int test_compressed_algorithm_end_to_end() {
    // Test the complete compression algorithm with the example from the description
    Timestamp sender = compressed_create(4, 2, CLOCK_COMPRESSED);
    CompressedClockData *sender_data = (CompressedClockData*)sender.data;
    
    // Set up the example: current vt = [5,8,1,0], last sent to P3 was [5,7,1,0]
    sender_data->vt[0] = 5;
    sender_data->vt[1] = 8;
    sender_data->vt[2] = 1;
    sender_data->vt[3] = 0;
    
    sender_data->tau[3][0] = 5;
    sender_data->tau[3][1] = 7;  // Different!
    sender_data->tau[3][2] = 1;
    sender_data->tau[3][3] = 0;
    
    // Serialize for destination 3
    int buffer[10];
    size_t msg_size = compressed_serialize_for_dest(&sender, 3, buffer, sizeof(buffer));
    
    // Should send only index 1 (7->8) and index 2 (1->2 after increment)
    TEST_ASSERT_EQ(5 * sizeof(int), msg_size, "Should send compressed format");
    TEST_ASSERT_EQ(2, buffer[0], "Should send 2 changed entries");
    
    // Create receiver and simulate receive
    Timestamp receiver = compressed_create(4, 3, CLOCK_COMPRESSED);
    compressed_merge(&receiver, buffer, msg_size);
    
    CompressedClockData *recv_data = (CompressedClockData*)receiver.data;
    
    // Receiver should have: incremented own clock, then merged
    TEST_ASSERT_EQ(1, recv_data->vt[3], "Receiver should increment own clock");
    TEST_ASSERT_EQ(8, recv_data->vt[1], "Receiver should have sender's vt[1]");
    TEST_ASSERT_EQ(2, recv_data->vt[2], "Receiver should have sender's vt[2]");
    
    compressed_destroy(&sender);
    compressed_destroy(&receiver);
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
    printf("=== Compressed Clock Test Suite ===\n\n");
    
    // Data Structure Tests
    printf("--- Data Structure Tests ---\n");
    RUN_TEST(test_compressed_create);
    RUN_TEST(test_compressed_destroy);
    RUN_TEST(test_compressed_clone);
    
    // Basic Operations Tests
    printf("\n--- Basic Operations Tests ---\n");
    RUN_TEST(test_compressed_increment);
    RUN_TEST(test_compressed_to_string);
    
    // Compression Algorithm Tests
    printf("\n--- Compression Algorithm Tests ---\n");
    RUN_TEST(test_compressed_serialize_for_dest_basic);
    RUN_TEST(test_compressed_serialize_for_dest_multiple_changes);
    RUN_TEST(test_compressed_serialize_for_dest_first_send);
    
    // Merge Tests
    printf("\n--- Merge Tests ---\n");
    RUN_TEST(test_compressed_merge_full_vector);
    RUN_TEST(test_compressed_merge_compressed_format);
    
    // Comparison Tests
    printf("\n--- Comparison Tests ---\n");
    RUN_TEST(test_compressed_compare);
    
    // Integration Tests
    printf("\n--- Integration Tests ---\n");
    RUN_TEST(test_compressed_algorithm_end_to_end);
    
    print_test_summary();
    
    return g_stats.tests_failed > 0 ? 1 : 0;
}