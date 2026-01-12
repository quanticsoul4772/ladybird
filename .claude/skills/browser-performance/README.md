# Browser Performance Skill

Comprehensive performance profiling and optimization guidance for Ladybird browser development.

## Quick Start

### Profile CPU Performance
```bash
# Profile page load
./scripts/profile_page_load.sh https://example.com

# Generate flame graph
./scripts/generate_flame_graph.sh perf.data flamegraph.svg
```

### Profile Memory
```bash
# Heap profiling
TOOL=heaptrack ./scripts/memory_profile.sh Ladybird

# Leak detection
TOOL=asan ./scripts/memory_profile.sh Ladybird
```

### Run Benchmarks
```bash
# Run full benchmark suite
./scripts/benchmark_suite.sh ./Build/release

# Compare before/after
./scripts/compare_profiles.sh baseline.json current.json
```

## Contents

### Main Documentation (SKILL.md)
- CPU profiling tools (perf, Instruments, callgrind, Tracy)
- Memory profiling tools (Heaptrack, Massif, ASAN)
- Browser-specific optimization patterns
- Benchmarking patterns
- Performance debugging workflow
- Common performance bottlenecks

### Examples
- `perf-workflow-example.sh` - Complete perf profiling workflow
- `flame-graph-example.sh` - Flame graph generation
- `benchmark-example.cpp` - Microbenchmark structure
- `optimization-example.cpp` - Before/after optimization patterns

### Scripts
- `profile_page_load.sh` - Profile Ladybird loading a URL
- `generate_flame_graph.sh` - Generate flame graphs from perf data
- `memory_profile.sh` - Memory profiling with multiple tools
- `benchmark_suite.sh` - Run benchmark suite and detect regressions
- `compare_profiles.sh` - Compare performance profiles

### References
- `profiling-tools.md` - Detailed tool comparison and usage
- `optimization-patterns.md` - Common optimization techniques
- `performance-checklist.md` - Pre-commit performance review
- `benchmarking-guide.md` - Creating good benchmarks
- `common-bottlenecks.md` - Frequent performance issues

## Workflow

1. **Profile** - Identify bottlenecks using perf/heaptrack
2. **Analyze** - Find root cause using flame graphs/reports
3. **Optimize** - Apply optimization patterns
4. **Verify** - Run benchmarks to measure improvement
5. **Prevent Regression** - Add benchmark tests

## Key Patterns Documented

### String Operations
- StringBuilder for concatenation
- StringView for temporary strings
- Avoiding copies in loops

### Layout/Rendering
- Avoiding layout thrashing
- Batching DOM operations
- Minimizing repaints

### CSS Performance
- Selector optimization
- Style computation caching
- Sharing computed styles

### Memory Optimization
- Vector capacity reservation
- Lazy initialization
- Object pooling

### IPC Optimization
- Batching messages
- Async communication
- Coalescing updates

## Tools Covered

| Tool | Platform | Type | Best For |
|------|----------|------|----------|
| perf | Linux | CPU | Production profiling |
| Instruments | macOS | CPU/Memory | macOS development |
| Callgrind | Linux/macOS | CPU | Detailed call graphs |
| Tracy | All | CPU/GPU | Real-time profiling |
| Heaptrack | Linux | Memory | Heap profiling |
| Massif | Linux/macOS | Memory | Heap snapshots |
| ASAN | All | Memory | Leak detection |

## Statistics

- **Total lines**: 4,000+ lines of documentation and scripts
- **Scripts**: 5 production-ready profiling scripts
- **Examples**: 4 comprehensive code examples
- **References**: 5 detailed reference documents
- **Patterns**: 13+ optimization patterns documented
- **Bottlenecks**: 12+ common issues covered

## Integration with Ladybird

This skill integrates with existing Ladybird infrastructure:
- Works with `./Meta/ladybird.py profile` command
- Compatible with CMake presets (Release, Sanitizers)
- Uses existing build system
- Follows Ladybird coding style

## Getting Help

- Check `SKILL.md` for comprehensive documentation
- Review `references/` for detailed guides
- Run example scripts in `examples/`
- Use `scripts/` for production profiling
