# Configurable Vector Clock System

C implementation of multiple vector clock algorithms for distributed system simulation.

## Features

- **Multiple Clock Implementations**: Standard, Sparse, Differential, Encoded, and Compressed vector clocks
- **Configurable Architecture**: Easy to add new clock types
- **Performance Comparison**: Built-in compression ratio analysis
- **Thread-Safe Simulation**: Multi-threaded distributed system simulation
- **Modular Design**: Clean separation of concerns across multiple files

## Supported Clock Types

| Type | Description | Compression | Use Case |
|------|-------------|-------------|----------|
| **Standard** | Full vector clocks (baseline) | 1.0x | Traditional implementation |
| **Sparse** | Only non-zero entries | Variable | Large sparse networks |
| **Differential** | Singhal-Kshemkalyani technique | 1.50x smaller | Frequent communication |
| **Encoded** | Prime number encoding | Variable | Small counter values |
| **Compressed** | True delta compression | Variable | Receiver-specific optimization |

## File Structure

### Header Files
- `timestamp.h` - Core types and abstract interface
- `standard_clock.h` - Standard vector clock interface
- `sparse_clock.h` - Sparse vector clock interface  
- `differential_clock.h` - Differential vector clock interface
- `encoded_clock.h` - Encoded vector clock interface
- `compressed_clock.h` - Compressed vector clock interface
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
- `compressed_clock.c` - Compressed vector clock implementation
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

See `CLAUDE.md` for detailed build commands and project structure information.

## Usage

```bash
# Basic usage
build/bin/vector_clock [num_processes] [steps_per_process] [clock_type]

# Examples
build/bin/vector_clock 5 20 1    # 5 processes, 20 steps, sparse clocks
build/bin/vector_clock 3 10 0    # 3 processes, 10 steps, standard clocks
build/bin/vector_clock --help    # Show help message
```

### Clock Type Parameters
- `0` - Standard vector clocks (baseline)
- `1` - Sparse vector clocks (compression)
- `2` - Differential vector clocks (Singhal-Kshemkalyani) 
- `3` - Encoded vector clocks (prime number encoding)
- `4` - Compressed vector clocks (true delta compression)

## Display Features

The system provides detailed event tracking with:

- **Step Tracking**: Shows step number for each simulation iteration
- **Before/After States**: Displays timestamp before and after each operation
- **Event Types**: INTERNAL, SEND, RECV with detailed state transitions
- **Comprehensive Logging**: Full trace of vector clock evolution

### Example Output
```
P0 Step0 INTERNAL(BEFORE) | TS=D[0,0] | local computation
P0 Step0 INTERNAL(AFTER)  | TS=D[1,0] | clock incremented
P1 Step1 SEND(BEFORE)     | TS=D[0,1] | to P0, payload="step 1: hello_from_P1_to_P0"
P1 Step1 SEND(AFTER)      | TS=D[0,2] | clock incremented and message sent
P0 Step2 RECV(BEFORE)     | TS=D[1,0] | from P1: payload="step 1: hello_from_P1_to_P0", msgTS=D[0,2]
P0 Step2 RECV(AFTER)      | TS=D[2,2] | merged with sender and incremented
```

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