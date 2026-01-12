# Performance Profiling Tools for Ladybird

## Quick Reference Table

| Tool | Platform | Type | Overhead | Visualization | Best For |
|------|----------|------|----------|---------------|----------|
| **perf** | Linux | CPU | Low (1-5%) | Flame graphs, text | Production profiling |
| **Instruments** | macOS | CPU/Memory | Low | GUI | macOS development |
| **Valgrind Callgrind** | Linux/macOS | CPU | High (10-50x) | KCachegrind | Detailed call graphs |
| **Tracy** | All | CPU/GPU | Low | Real-time GUI | Development profiling |
| **Heaptrack** | Linux | Memory | Low | GUI | Heap profiling |
| **Valgrind Massif** | Linux/macOS | Memory | High | ms_print, GUI | Heap snapshots |
| **ASAN** | All | Memory | Medium | Text logs | Leak detection |

## Linux perf

### Installation
```bash
# Ubuntu/Debian
sudo apt install linux-tools-common linux-tools-generic linux-tools-`uname -r`

# Arch
sudo pacman -S perf

# Allow non-root profiling
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
echo 0 | sudo tee /proc/sys/kernel/kptr_restrict
```

### Basic Usage
```bash
# Record CPU profile
perf record -F 99 -g -- ./Ladybird

# View results
perf report

# Record with higher frequency (more accurate, higher overhead)
perf record -F 999 -g -- ./Ladybird

# Record with DWARF call graphs (best accuracy)
perf record -F 99 -g --call-graph=dwarf -- ./Ladybird
```

### Advanced Features
```bash
# Profile specific CPU events
perf record -e cycles,instructions,cache-misses -g -- ./Ladybird

# Profile all processes (requires root)
sudo perf record -F 99 -g -a

# Attach to running process
perf record -F 99 -g -p $(pgrep WebContent)

# Profile for specific duration
perf record -F 99 -g -- sleep 30 & perf record -F 99 -g -p $!
```

### Analysis Commands
```bash
# Interactive TUI
perf report

# Text output (for scripts)
perf report --stdio

# Show call graph
perf report --stdio -g graph

# Show annotated source
perf annotate <function_name>

# Differential profile
perf diff perf.data.old perf.data
```

### Flame Graphs
```bash
# Install FlameGraph
git clone https://github.com/brendangregg/FlameGraph
export PATH=$PATH:$PWD/FlameGraph

# Generate flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg

# Differential flame graph
perf script -i old.data | stackcollapse-perf.pl > old.folded
perf script -i new.data | stackcollapse-perf.pl > new.folded
difffolded.pl old.folded new.folded | flamegraph.pl > diff.svg
```

### Event Types
```bash
# List available events
perf list

# Common hardware events
perf record -e cycles          # CPU cycles
perf record -e instructions    # Instructions retired
perf record -e cache-misses    # Cache misses
perf record -e branch-misses   # Branch mispredictions
perf record -e page-faults     # Page faults

# Software events
perf record -e cpu-clock       # CPU clock timer
perf record -e task-clock      # Task clock
perf record -e context-switches # Context switches
```

## macOS Instruments

### Launch from Command Line
```bash
# Time Profiler
xcrun xctrace record --template 'Time Profiler' \
    --launch ./Ladybird.app \
    --output ladybird.trace

# Allocations (memory)
xcrun xctrace record --template 'Allocations' \
    --launch ./Ladybird.app \
    --output ladybird_memory.trace

# Leaks
xcrun xctrace record --template 'Leaks' \
    --launch ./Ladybird.app \
    --output ladybird_leaks.trace

# View results
open ladybird.trace
```

### Manual Profiling
1. Open Instruments.app
2. Select template (Time Profiler, Allocations, Leaks, etc.)
3. Choose Ladybird as target
4. Click Record
5. Perform operations to profile
6. Stop and analyze results

### Analysis Features
- **Call Tree**: View function call hierarchy
- **Heaviest Stack Trace**: Find most expensive paths
- **Timeline**: See CPU usage over time
- **Flame Graph**: Visualize call stacks
- **Symbol Source**: Jump to source code

## Valgrind Callgrind

### Installation
```bash
# Ubuntu/Debian
sudo apt install valgrind kcachegrind

# macOS
brew install valgrind --HEAD  # Experimental support
```

### Basic Usage
```bash
# Profile with callgrind
valgrind --tool=callgrind \
    --callgrind-out-file=callgrind.out \
    ./Ladybird

# Visualize with KCachegrind
kcachegrind callgrind.out
```

### Advanced Options
```bash
# Detailed profiling (slow!)
valgrind --tool=callgrind \
    --dump-instr=yes \
    --collect-jumps=yes \
    --cache-sim=yes \
    --branch-sim=yes \
    ./Ladybird

# Start with profiling disabled (enable on demand)
valgrind --tool=callgrind --instr-atstart=no ./Ladybird &

# Control profiling dynamically
callgrind_control -i on <pid>   # Enable instrumentation
callgrind_control -d <pid>      # Dump statistics
callgrind_control -i off <pid>  # Disable instrumentation
```

### Analyzing Output
```bash
# Text summary
callgrind_annotate callgrind.out

# Compare two profiles
callgrind_annotate --auto=yes \
    --old=callgrind.out.old \
    callgrind.out
```

### KCachegrind Features
- **Flat Profile**: Function-level statistics
- **Call Graph**: Caller/callee relationships
- **Source Code**: Annotated source with costs
- **Instruction Counts**: Detailed CPU instruction analysis
- **Cache Simulation**: Cache miss analysis

## Tracy Profiler

### Installation
```bash
# Build Tracy server
git clone https://github.com/wolfpld/tracy.git
cd tracy/profiler/build/unix
make release

# Copy server to PATH
sudo cp Tracy-release /usr/local/bin/tracy
```

### Integrate into Ladybird
```cmake
# CMakeLists.txt
option(ENABLE_TRACY "Enable Tracy profiling" OFF)

if(ENABLE_TRACY)
    add_compile_definitions(TRACY_ENABLE)
    include_directories(${CMAKE_SOURCE_DIR}/ThirdParty/tracy/public)

    # Add Tracy client library
    add_library(TracyClient STATIC
        ${CMAKE_SOURCE_DIR}/ThirdParty/tracy/public/TracyClient.cpp
    )
endif()
```

### Instrument Code
```cpp
#include <tracy/Tracy.hpp>

void render_page() {
    ZoneScoped;  // Automatic profiling of this function
    // ... code ...
}

void complex_operation() {
    ZoneScopedN("Complex Operation");  // Named zone

    {
        ZoneScopedN("Phase 1");
        // ... phase 1 ...
    }

    {
        ZoneScopedN("Phase 2");
        // ... phase 2 ...
    }
}

// Frame marking (for frame-based profiling)
while (running) {
    FrameMark;  // Mark frame boundary
    render_frame();
}
```

### Usage
```bash
# Build with Tracy
cmake -B Build -DENABLE_TRACY=ON
cmake --build Build

# Run Tracy server
tracy &

# Run Ladybird (auto-connects to Tracy)
./Build/release/bin/Ladybird
```

### Tracy Features
- **Real-time profiling**: See results as they happen
- **Frame profiling**: Analyze frame times
- **Memory profiling**: Track allocations
- **Plot values**: Graph custom metrics
- **Compare traces**: Before/after comparison

## Heaptrack

### Installation
```bash
# Ubuntu/Debian
sudo apt install heaptrack heaptrack-gui

# Arch
sudo pacman -S heaptrack
```

### Basic Usage
```bash
# Profile heap allocations
heaptrack ./Ladybird

# Output: heaptrack.ladybird.XXXXX.gz

# Analyze in GUI
heaptrack_gui heaptrack.ladybird.*.gz

# Text summary
heaptrack_print heaptrack.ladybird.*.gz
```

### Advanced Options
```bash
# Profile specific process by PID
heaptrack -p $(pgrep WebContent)

# Profile with custom output name
heaptrack -o myprofile.gz ./Ladybird

# Disable tracking after launch (attach on demand)
heaptrack --disable ./Ladybird
```

### Analysis
- **Peak heap usage**: Maximum memory allocated
- **Total allocations**: Number of allocations
- **Temporary allocations**: Short-lived objects
- **Flame graph**: Allocation call stacks
- **Timeline**: Memory usage over time

## Valgrind Massif

### Installation
```bash
# Same as Callgrind
sudo apt install valgrind
```

### Basic Usage
```bash
# Profile heap memory
valgrind --tool=massif \
    --massif-out-file=massif.out \
    ./Ladybird

# View results
ms_print massif.out

# GUI visualization
massif-visualizer massif.out
```

### Advanced Options
```bash
# Detailed snapshots
valgrind --tool=massif \
    --time-unit=ms \
    --detailed-freq=1 \
    --max-snapshots=200 \
    --threshold=0.1 \
    ./Ladybird

# Profile stack and heap
valgrind --tool=massif \
    --stacks=yes \
    ./Ladybird
```

### Output Format
```
--------------------------------------------------------------------------------
  n        time(ms)         total(B)   useful-heap(B) extra-heap(B)    stacks(B)
--------------------------------------------------------------------------------
 10        1,000           10,485,760      10,240,000       245,760            0
 20        2,000           20,971,520      20,480,000       491,520            0
```

## AddressSanitizer (ASAN)

### Build with ASAN
```bash
# Ladybird has Sanitizers preset
cmake --preset Sanitizers
cmake --build Build/sanitizers
```

### Usage
```bash
# Run with leak detection
ASAN_OPTIONS=detect_leaks=1 \
LSAN_OPTIONS=report_objects=1 \
    ./Build/sanitizers/bin/Ladybird
```

### ASAN Options
```bash
# Verbose output
ASAN_OPTIONS=verbosity=1

# Save logs to file
ASAN_OPTIONS=log_path=asan.log

# Check initialization order
ASAN_OPTIONS=check_initialization_order=1

# Detect stack use after return
ASAN_OPTIONS=detect_stack_use_after_return=1

# Print statistics
ASAN_OPTIONS=print_stats=1
```

### Suppressions
```bash
# Create suppressions file
cat > lsan.supp << EOF
leak:libfontconfig
leak:libfreetype
EOF

# Use suppressions
LSAN_OPTIONS=suppressions=lsan.supp ./Build/sanitizers/bin/Ladybird
```

## Tool Selection Guide

### Choose Based on Need

**Quick CPU profiling** → perf (Linux) or Instruments (macOS)
- Fast, low overhead
- Production-safe
- Good visualization

**Detailed call graph** → Valgrind Callgrind
- Precise instruction counts
- Deterministic results
- High overhead (development only)

**Real-time profiling** → Tracy
- Live results
- Frame-based analysis
- Requires code instrumentation

**Memory usage** → Heaptrack
- Fast heap profiling
- Allocation tracking
- Good visualization

**Memory leak detection** → ASAN
- Precise leak detection
- Stack traces
- Runtime overhead acceptable

**Memory snapshots** → Valgrind Massif
- Heap growth over time
- Stack usage
- High overhead

## Common Workflows

### Profile Page Load
```bash
# CPU profiling
perf record -F 99 -g -- ./Ladybird https://example.com
perf report

# Memory profiling
heaptrack ./Ladybird https://example.com
heaptrack_gui heaptrack.*.gz
```

### Find Memory Leak
```bash
# Build with ASAN
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Run with leak detection
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

# Check ASAN output for leaks
```

### Optimize Hot Function
```bash
# 1. Identify hot function
perf record -F 999 -g -- ./Ladybird
perf report --stdio | grep <function>

# 2. Profile just that function
perf record -e cycles -g -- ./benchmark <function>

# 3. Annotate assembly
perf annotate <function>

# 4. Make changes, re-profile
perf record -e cycles -g -- ./benchmark <function>
perf diff perf.data.old perf.data
```

### Compare Before/After
```bash
# Baseline
git checkout main
perf record -F 99 -g -o baseline.data -- ./Ladybird
perf script -i baseline.data | stackcollapse-perf.pl > baseline.folded

# After changes
git checkout feature
perf record -F 99 -g -o feature.data -- ./Ladybird
perf script -i feature.data | stackcollapse-perf.pl > feature.folded

# Differential flame graph
difffolded.pl baseline.folded feature.folded | flamegraph.pl > diff.svg
```
