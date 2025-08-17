# Configurable Vector Clock System

C implementation of multiple vector clock algorithms for distributed system simulation.

## Features

- **Multiple Clock Implementations**: Standard, Sparse, Differential, and Encoded vector clocks
- **Configurable Architecture**: Easy to add new clock types
- **Performance Comparison**: Built-in compression ratio analysis
- **Thread-Safe Simulation**: Multi-threaded distributed system simulation
- **Modular Design**: Clean separation of concerns across multiple files

## Supported Clock Types

| Type | Description | Compression | Use Case |
|------|-------------|-------------|----------|
| **Standard** | Full vector clocks (baseline) | 1.0x | Traditional implementation |
| **Sparse** | Only non-zero entries | 1.5x smaller | Large sparse networks |
| **Differential** | Singhal-Kshemkalyani technique | 1.0x | FIFO communication |
| **Encoded** | Prime number encoding | 1.5x smaller | Small counter values |

## File Structure

### Header Files
- `timestamp.h` - Core types and abstract interface
- `standard_clock.h` - Standard vector clock interface
- `sparse_clock.h` - Sparse vector clock interface  
- `differential_clock.h` - Differential vector clock interface
- `encoded_clock.h` - Encoded vector clock interface
- `message_queue.h` - Thread-safe message queue
- `simulation.h` - Simulation framework
- `config.h` - Configuration constants

### Implementation Files
- `main.c` - Entry point and demo driver
- `timestamp.c` - Main timestamp interface dispatch
- `standard_clock.c` - Standard vector clock implementation
- `sparse_clock.c` - Sparse vector clock implementation
- `differential_clock.c` - Differential vector clock implementation
- `encoded_clock.c` - Prime number encoded vector clock
- `message_queue.c` - Thread-safe message queue
- `simulation.c` - Simulation framework and worker threads

## Building

```bash
# Build the simulator
make

# Build with debug symbols
make debug

# Run comprehensive tests
make test

# Clean build artifacts
make clean

# Show available targets
make help
```

## Usage

```bash
# Basic usage
./vector_clock [num_processes] [steps_per_process] [clock_type]

# Examples
./vector_clock 5 20 1    # 5 processes, 20 steps, sparse clocks
./vector_clock 3 10 0    # 3 processes, 10 steps, standard clocks
./vector_clock --help    # Show help message
```

### Clock Type Parameters
- `0` - Standard vector clocks (baseline)
- `1` - Sparse vector clocks (compression)
- `2` - Differential vector clocks (Singhal-Kshemkalyani) 
- `3` - Encoded vector clocks (prime number encoding)

## Architecture

The system uses an abstract `Timestamp` interface with pluggable `TimestampOps` function pointers, allowing runtime selection of clock implementations. Each clock type provides:

- `create/destroy` - Lifecycle management
- `increment` - Local event handling  
- `merge` - Message reception and vector merging
- `compare` - Causal relationship determination
- `serialize/deserialize` - Network communication
- `to_string` - Display formatting
- `clone` - Deep copying

## Performance Analysis

The system provides detailed performance statistics including:
- Total messages sent and bytes transferred
- Average and maximum timestamp sizes
- Compression ratios compared to standard vector clocks
- Memory usage analysis

## Adding New Clock Types

1. Create header file `new_clock.h` with data structures and interface
2. Implement operations in `new_clock.c`
3. Add to `ClockType` enum in `timestamp.h`
4. Register operations table in `timestamp.c`
5. Update `clock_type_names` and `clock_type_descriptions`

## References

- Lamport, L. "Time, Clocks, and the Ordering of Events in a Distributed System"
- Singhal, M. and Kshemkalyani, A. "An Efficient Implementation of Vector Clocks"
- Prime number encoding for vector timestamp compression techniques