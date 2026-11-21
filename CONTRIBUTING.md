# Contributing to AeroMatch

Thank you for your interest in contributing to AeroMatch, the high-performance C++20 limit order book matching engine developed by Apex Trading Technologies.

## Code of Conduct

Please treat all contributors with respect, professionalism, and constructive collaboration.

## Development Workflow

### Prerequisites
- Modern C++ compiler supporting C++20 (GCC 13+ or Clang 16+)
- CMake 3.20 or newer
- Ninja or Make

### Building and Testing

```bash
# Configure release build with unit tests
cmake -B build -DCMAKE_BUILD_TYPE=Release -DAEROMATCH_BUILD_TESTS=ON

# Compile core engine and test suite
cmake --build build -j$(nproc)

# Run test discovery suite
ctest --test-dir build --output-on-failure
```

### Running Benchmarks

```bash
# Enable benchmarks in CMake
cmake -B build -DAEROMATCH_BUILD_BENCHMARKS=ON
cmake --build build --target bench_matching bench_memory bench_queue

# Execute throughput and latency benchmarks
./build/benchmarks/bench_matching
```

## Architectural Guidelines

1. **Zero Heap Allocations on Critical Path**:
   - Order placement, matching, and cancellation must not allocate dynamic heap memory.
   - Utilize pre-allocated `ObjectPool<Order>` and intrusive doubly-linked price level lists.
2. **Deterministic Arithmetic Safety**:
   - Use checked arithmetic (e.g. `__uint128_t` for `price * quantity` notional checks) to guard against integer overflow.
3. **Pluggable Event Streaming**:
   - Engine components must remain decoupled from specific external message brokers. Use `ITradeEventTap` to export trades or events.
4. **Cache Friendliness**:
   - Maintain 64-byte alignment on the `Order` struct.
5. **No Database Dependencies in Core**:
   - The open-source engine operates purely in-memory. Never introduce database client dependencies to the core matching library.

## Pull Request Process

1. Fork the repository and create a feature branch (`git checkout -b feature/my-enhancement`).
2. Implement your changes following modern C++20 conventions.
3. Ensure all unit tests pass with zero failures.
4. Add new GoogleTest test cases covering your functionality or edge cases.
5. Submit a pull request with a clear description of the problem solved, design rationale, and benchmark verification.
