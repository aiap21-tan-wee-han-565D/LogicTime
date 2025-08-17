# Vector Clock Simulation Makefile
# Configurable implementation supporting multiple clock types

# Compiler and flags
CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99 -pthread -Iinclude
LDFLAGS = -pthread

# Directories
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Target executable
TARGET = $(BIN_DIR)/vector_clock

# Source files (with paths)
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/timestamp.c $(SRC_DIR)/standard_clock.c $(SRC_DIR)/sparse_clock.c $(SRC_DIR)/differential_clock.c $(SRC_DIR)/encoded_clock.c $(SRC_DIR)/compressed_clock.c $(SRC_DIR)/message_queue.c $(SRC_DIR)/simulation.c

# Test source files
TEST_SOURCES = $(TEST_DIR)/test_differential_clock.c $(SRC_DIR)/differential_clock.c
TEST_DEPS = $(SRC_DIR)/standard_clock.c $(SRC_DIR)/sparse_clock.c $(SRC_DIR)/encoded_clock.c $(SRC_DIR)/compressed_clock.c $(SRC_DIR)/timestamp.c

# Compressed clock test source files
COMPRESSED_TEST_SOURCES = $(TEST_DIR)/test_compressed_clock.c $(SRC_DIR)/compressed_clock.c
COMPRESSED_TEST_DEPS = $(SRC_DIR)/standard_clock.c $(SRC_DIR)/sparse_clock.c $(SRC_DIR)/encoded_clock.c $(SRC_DIR)/differential_clock.c $(SRC_DIR)/timestamp.c

# Header files
HEADERS = $(INCLUDE_DIR)/timestamp.h $(INCLUDE_DIR)/standard_clock.h $(INCLUDE_DIR)/sparse_clock.h $(INCLUDE_DIR)/differential_clock.h $(INCLUDE_DIR)/encoded_clock.h $(INCLUDE_DIR)/compressed_clock.h $(INCLUDE_DIR)/message_queue.h $(INCLUDE_DIR)/simulation.h $(INCLUDE_DIR)/config.h

# Object files (in build directory)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Test object files
TEST_SRC_OBJS = $(filter $(SRC_DIR)/%.c,$(TEST_SOURCES))
TEST_SRC_OBJS := $(TEST_SRC_OBJS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_DIR_OBJS = $(filter $(TEST_DIR)/%.c,$(TEST_SOURCES))
TEST_DIR_OBJS := $(TEST_DIR_OBJS:$(TEST_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_DEP_OBJS = $(TEST_DEPS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_OBJECTS = $(TEST_SRC_OBJS) $(TEST_DIR_OBJS) $(TEST_DEP_OBJS)

# Compressed test object files
COMPRESSED_TEST_SRC_OBJS = $(filter $(SRC_DIR)/%.c,$(COMPRESSED_TEST_SOURCES))
COMPRESSED_TEST_SRC_OBJS := $(COMPRESSED_TEST_SRC_OBJS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
COMPRESSED_TEST_DIR_OBJS = $(filter $(TEST_DIR)/%.c,$(COMPRESSED_TEST_SOURCES))
COMPRESSED_TEST_DIR_OBJS := $(COMPRESSED_TEST_DIR_OBJS:$(TEST_DIR)/%.c=$(OBJ_DIR)/%.o)
COMPRESSED_TEST_DEP_OBJS = $(COMPRESSED_TEST_DEPS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
COMPRESSED_TEST_OBJECTS = $(COMPRESSED_TEST_SRC_OBJS) $(COMPRESSED_TEST_DIR_OBJS) $(COMPRESSED_TEST_DEP_OBJS)

# Default target
all: $(TARGET)

# Create build directories
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# Link target
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c $(HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Build test executable for differential clock
$(BIN_DIR)/test_differential_clock: $(TEST_OBJECTS) | $(BIN_DIR)
	$(CC) $(TEST_OBJECTS) -o $@ $(LDFLAGS)

# Run differential clock unit tests
test-differential: $(BIN_DIR)/test_differential_clock
	@echo "Running Differential Clock Unit Tests:"
	$(BIN_DIR)/test_differential_clock

# Build test executable for compressed clock
$(BIN_DIR)/test_compressed_clock: $(COMPRESSED_TEST_OBJECTS) | $(BIN_DIR)
	$(CC) $(COMPRESSED_TEST_OBJECTS) -o $@ $(LDFLAGS)

# Run compressed clock unit tests
test-compressed: $(BIN_DIR)/test_compressed_clock
	@echo "Running Compressed Clock Unit Tests:"
	$(BIN_DIR)/test_compressed_clock

# Run tests with different clock types
test: $(TARGET)
	@echo "Testing Standard Vector Clocks:"
	$(TARGET) 3 5 0
	@echo "\nTesting Sparse Vector Clocks:"
	$(TARGET) 3 5 1
	@echo "\nTesting Differential Vector Clocks:"
	$(TARGET) 3 5 2
	@echo "\nTesting Encoded Vector Clocks:"
	$(TARGET) 3 5 3
	@echo "\nTesting Compressed Vector Clocks:"
	$(TARGET) 3 5 4

# Run all tests (integration + unit)
test-all: test test-differential test-compressed

# Show help
help:
	@echo "Available targets:"
	@echo "  all              - Build the vector clock simulator (default)"
	@echo "  clean            - Remove build artifacts"
	@echo "  debug            - Build with debugging symbols"
	@echo "  test             - Run integration tests with all clock types"
	@echo "  test-differential - Run differential clock unit tests"
	@echo "  test-compressed  - Run compressed clock unit tests"
	@echo "  test-all         - Run both integration and unit tests"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Project structure:"
	@echo "  include/         - Header files"
	@echo "  src/             - Source files"
	@echo "  tests/           - Test files"
	@echo "  build/           - Build artifacts (auto-generated)"

# Declare phony targets
.PHONY: all clean debug test test-differential test-compressed test-all help