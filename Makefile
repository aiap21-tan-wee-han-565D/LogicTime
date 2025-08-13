# Vector Clock Simulation Makefile
# Configurable implementation supporting multiple clock types

# Compiler and flags
CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99 -pthread
LDFLAGS = -pthread

# Target executable
TARGET = vector_clock

# Source files
SOURCES = main.c timestamp.c standard_clock.c sparse_clock.c differential_clock.c encoded_clock.c message_queue.c simulation.c

# Header files
HEADERS = timestamp.h standard_clock.h sparse_clock.h differential_clock.h encoded_clock.h message_queue.h simulation.h config.h

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Link target
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Run tests with different clock types
test: $(TARGET)
	@echo "Testing Standard Vector Clocks:"
	./$(TARGET) 3 5 0
	@echo "\nTesting Sparse Vector Clocks:"
	./$(TARGET) 3 5 1
	@echo "\nTesting Differential Vector Clocks:"
	./$(TARGET) 3 5 2
	@echo "\nTesting Encoded Vector Clocks:"
	./$(TARGET) 3 5 3

# Show help
help:
	@echo "Available targets:"
	@echo "  all     - Build the vector clock simulator (default)"
	@echo "  clean   - Remove build artifacts"
	@echo "  debug   - Build with debugging symbols"
	@echo "  test    - Run tests with all clock types"
	@echo "  help    - Show this help message"

# Declare phony targets
.PHONY: all clean debug test help