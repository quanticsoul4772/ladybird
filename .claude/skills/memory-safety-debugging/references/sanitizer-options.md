# Sanitizer Options Reference

Complete reference for sanitizer environment variables and configuration.

## AddressSanitizer (ASAN) Options

Set via `ASAN_OPTIONS` environment variable. Options are colon-separated.

### Core Detection Options

```bash
export ASAN_OPTIONS="\
detect_leaks=1:\
strict_string_checks=1:\
detect_stack_use_after_return=1:\
check_initialization_order=1:\
strict_init_order=1:\
detect_invalid_pointer_pairs=2"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `detect_leaks` | 0/1 | 1 | Enable LeakSanitizer |
| `strict_string_checks` | 0/1 | 0 | Check strcpy/strcat/etc more strictly |
| `detect_stack_use_after_return` | 0/1 | 0 | Detect use-after-return (slower) |
| `check_initialization_order` | 0/1 | 0 | Detect init-order bugs |
| `strict_init_order` | 0/1 | 0 | Die on init-order bugs |
| `detect_invalid_pointer_pairs` | 0/1/2 | 0 | Detect pointer comparison/subtraction (2=more strict) |

### Output Options

```bash
export ASAN_OPTIONS="\
print_stacktrace=1:\
symbolize=1:\
log_path=asan.log:\
log_to_syslog=0:\
verbosity=0"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `print_stacktrace` | 0/1 | 1 | Print stack trace on error |
| `symbolize` | 0/1 | 1 | Symbolicate stack traces |
| `log_path` | path | stderr | Log file path (adds .PID suffix) |
| `log_to_syslog` | 0/1 | 0 | Send logs to syslog |
| `verbosity` | 0-2 | 0 | Verbosity level |

### Error Handling Options

```bash
export ASAN_OPTIONS="\
halt_on_error=0:\
abort_on_error=0:\
exitcode=1:\
error_limit=0"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `halt_on_error` | 0/1 | 0 | Stop execution on first error |
| `abort_on_error` | 0/1 | 0 | Call abort() on error |
| `exitcode` | int | 1 | Exit code on error |
| `error_limit` | int | 0 | Max errors before exiting (0=unlimited) |

### Suppression Options

```bash
export ASAN_OPTIONS="suppressions=/path/to/asan.supp"
```

**asan.supp example:**
```
# Suppress leak in third-party library
leak:libfontconfig.so

# Suppress leak in specific function
leak:QApplication::exec

# Suppress use-after-free in Qt
interceptor_via_fun:QWidget::show
```

### Performance Options

```bash
export ASAN_OPTIONS="\
malloc_context_size=20:\
fast_unwind_on_malloc=0:\
quarantine_size_mb=256:\
redzone=16"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `malloc_context_size` | int | 30 | Stack trace depth for malloc |
| `fast_unwind_on_malloc` | 0/1 | 1 | Use fast unwinder (less accurate) |
| `quarantine_size_mb` | int | 256 | Quarantine size in MB |
| `redzone` | int | 16 | Size of redzone around allocations |

### Symbolizer Options

```bash
export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer
```

Set path to symbolizer binary for stack trace symbolication.

## UndefinedBehaviorSanitizer (UBSAN) Options

Set via `UBSAN_OPTIONS` environment variable.

### Core Options

```bash
export UBSAN_OPTIONS="\
print_stacktrace=1:\
halt_on_error=0:\
report_error_type=1"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `print_stacktrace` | 0/1 | 0 | Print stack trace on error |
| `halt_on_error` | 0/1 | 0 | Stop on first error |
| `report_error_type` | 0/1 | 1 | Include error type in report |

### Suppression Options

```bash
export UBSAN_OPTIONS="suppressions=/path/to/ubsan.supp"
```

**ubsan.supp example:**
```
# Suppress signed overflow in third-party code
signed-integer-overflow:qhash.cpp

# Suppress null pointer in Qt
null-pointer-dereference:QWidget.cpp
```

### Specific Checks

UBSAN checks are usually enabled at compile time. Common checks:

- `signed-integer-overflow` - Signed integer overflow
- `unsigned-integer-overflow` - Unsigned integer overflow
- `shift-base` - Shift base is negative or too large
- `shift-exponent` - Shift exponent is negative or too large
- `division-by-zero` - Integer division by zero
- `null` - Null pointer dereference
- `return` - Return value check
- `bounds` - Array bounds check
- `alignment` - Misaligned pointer use
- `object-size` - Object size check
- `vptr` - Virtual function table check
- `enum` - Invalid enum value

## LeakSanitizer (LSAN) Options

Set via `LSAN_OPTIONS` environment variable. LSan is part of ASan.

### Core Options

```bash
export LSAN_OPTIONS="\
print_suppressions=0:\
use_stacks=1:\
use_registers=1:\
use_globals=1:\
use_tls=1"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `print_suppressions` | 0/1 | 1 | Print matched suppressions |
| `use_stacks` | 0/1 | 1 | Search stack for pointers |
| `use_registers` | 0/1 | 1 | Search registers for pointers |
| `use_globals` | 0/1 | 1 | Search globals for pointers |
| `use_tls` | 0/1 | 1 | Search TLS for pointers |

### Suppression Options

```bash
export LSAN_OPTIONS="suppressions=/path/to/lsan.supp"
```

**lsan.supp example:**
```
# Suppress leak in Qt event loop
leak:QEventLoop::exec

# Suppress leak in OpenGL driver
leak:libGL.so

# Suppress leak in fontconfig
leak:FcConfigSubstitute
```

## ThreadSanitizer (TSAN) Options

Set via `TSAN_OPTIONS` environment variable.

### Core Options

```bash
export TSAN_OPTIONS="\
second_deadlock_stack=1:\
history_size=7:\
io_sync=1"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `second_deadlock_stack` | 0/1 | 0 | Show second stack in deadlock reports |
| `history_size` | 0-7 | 2 | Size of per-thread history buffer |
| `io_sync` | 0/1 | 1 | Treat I/O as synchronization |

### Suppression Options

```bash
export TSAN_OPTIONS="suppressions=/path/to/tsan.supp"
```

**tsan.supp example:**
```
# Suppress race in third-party library
race:libQt5Core.so

# Suppress race in specific function
race:ResourceLoader::did_load
```

## MemorySanitizer (MSAN) Options

Set via `MSAN_OPTIONS` environment variable.

### Core Options

```bash
export MSAN_OPTIONS="\
poison_in_dtor=1:\
print_stats=0"
```

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `poison_in_dtor` | 0/1 | 0 | Poison memory in destructors |
| `print_stats` | 0/1 | 0 | Print statistics on exit |

## Combined Configuration

Example comprehensive configuration for development:

```bash
# ~/.bashrc or project script

# ASAN configuration
export ASAN_OPTIONS="\
detect_leaks=1:\
strict_string_checks=1:\
detect_stack_use_after_return=1:\
check_initialization_order=1:\
strict_init_order=1:\
detect_invalid_pointer_pairs=2:\
print_stacktrace=1:\
symbolize=1:\
halt_on_error=0:\
abort_on_error=0:\
log_path=asan.log:\
suppressions=$HOME/ladybird/asan_suppressions.txt"

# UBSAN configuration
export UBSAN_OPTIONS="\
print_stacktrace=1:\
halt_on_error=0:\
suppressions=$HOME/ladybird/ubsan_suppressions.txt"

# LSAN configuration
export LSAN_OPTIONS="\
suppressions=$HOME/ladybird/lsan_suppressions.txt:\
print_suppressions=0"

# Symbolizer path
export ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer)
```

## CI Configuration

Example for Continuous Integration:

```bash
# CI script - halt on first error

export ASAN_OPTIONS="\
detect_leaks=1:\
strict_string_checks=1:\
print_stacktrace=1:\
halt_on_error=1:\
abort_on_error=1:\
log_path=ci_asan.log"

export UBSAN_OPTIONS="\
print_stacktrace=1:\
halt_on_error=1"

# Run tests
ctest --preset Sanitizers
```

## Debugging Configuration

Example for debugging specific issues:

```bash
# More verbose output, stop on first error

export ASAN_OPTIONS="\
detect_leaks=1:\
verbosity=1:\
print_stacktrace=1:\
halt_on_error=1:\
malloc_context_size=30:\
log_path=debug_asan.log"

# Run in GDB
gdb --args ./Build/sanitizers/bin/Ladybird
```

## Performance Tuning

For faster execution (less thorough checking):

```bash
export ASAN_OPTIONS="\
detect_leaks=0:\
detect_stack_use_after_return=0:\
fast_unwind_on_malloc=1:\
quarantine_size_mb=64"
```

For more thorough checking (slower):

```bash
export ASAN_OPTIONS="\
detect_leaks=1:\
detect_stack_use_after_return=1:\
fast_unwind_on_malloc=0:\
malloc_context_size=50:\
quarantine_size_mb=512"
```

## Common Patterns

### Development Workflow

```bash
# 1. Initial run - collect all errors
ASAN_OPTIONS="halt_on_error=0:log_path=all_errors.log" ./Build/sanitizers/bin/Ladybird

# 2. Focus on first error
ASAN_OPTIONS="halt_on_error=1" ./Build/sanitizers/bin/Ladybird

# 3. Debug with GDB
ASAN_OPTIONS="halt_on_error=1" gdb --args ./Build/sanitizers/bin/Ladybird
```

### Testing Workflow

```bash
# 1. Run all tests, collect failures
ASAN_OPTIONS="halt_on_error=0:log_path=test_failures.log" ctest

# 2. Run specific test
ASAN_OPTIONS="halt_on_error=1" ./Build/sanitizers/bin/TestIPC

# 3. Analyze failures
./scripts/analyze_sanitizer_report.sh test_failures.log.*
```

## References

- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [UBSan Documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [ThreadSanitizer Documentation](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [MemorySanitizer Documentation](https://github.com/google/sanitizers/wiki/MemorySanitizer)
