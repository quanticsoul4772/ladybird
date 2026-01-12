---
name: fuzzing-workflow
description: Complete workflow for fuzzing Ladybird browser components using AFL++, LibFuzzer, and custom harnesses. Includes corpus management, crash triage, and sanitizer integration.
use-when: Setting up fuzzing campaigns, writing fuzz targets, triaging crashes, or improving test coverage
allowed-tools:
  - Bash
  - Read
  - Write
  - Grep
scripts:
  - run_fuzzer.sh
  - triage_crashes.sh
  - minimize_corpus.sh
---

# Fuzzing Workflow for Ladybird

## Overview
```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│ Write Fuzz  │ → │ Run Campaign │ → │   Triage    │
│   Target    │    │  (AFL++/LF)  │    │   Crashes   │
└─────────────┘    └──────────────┘    └─────────────┘
                            ↓                   ↓
                    ┌──────────────┐    ┌─────────────┐
                    │   Monitor    │ ← │  Minimize   │
                    │   Coverage   │    │  Reproducer │
                    └──────────────┘    └─────────────┘
```

## 1. Writing Fuzz Targets

### IPC Message Fuzzer (Primary Focus)
```cpp
// Ladybird/Fuzzing/FuzzIPCMessages.cpp
#include <LibIPC/Decoder.h>
#include <LibIPC/Message.h>
#include <Services/WebContent/WebContentClient.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Initialize decoder with fuzz data
    IPC::Decoder decoder(data, size);

    // Try to decode message type
    auto message_type = decoder.decode<u32>();
    if (!message_type.has_value())
        return 0;

    // Create mock client
    auto client = WebContent::WebContentClient::create();

    // Attempt to handle message
    // Crashes/hangs/sanitizer violations are bugs
    (void)client->handle_message(*message_type, decoder);

    return 0;
}
```

### HTML Parser Fuzzer
```cpp
// Ladybird/Fuzzing/FuzzHTMLParser.cpp
#include <LibWeb/HTML/Parser/HTMLParser.h>
#include <LibWeb/DOM/Document.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    auto input = StringView(reinterpret_cast<char const*>(data), size);

    // Create document
    auto document = Web::DOM::Document::create();

    // Parse HTML
    Web::HTML::HTMLParser parser(document, input);
    (void)parser.run();

    return 0;
}
```

### Image Decoder Fuzzer
```cpp
// Ladybird/Fuzzing/FuzzImageDecoder.cpp
#include <LibGfx/ImageDecoder.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    auto buffer = ByteBuffer::copy(data, size);
    if (!buffer.has_value())
        return 0;

    // Try all image formats
    (void)Gfx::ImageDecoder::try_create_for_raw_bytes(buffer.value());

    return 0;
}
```

## 2. Building Fuzz Targets

### CMake Configuration
```cmake
# Ladybird/Fuzzing/CMakeLists.txt
if(ENABLE_FUZZING)
    # IPC fuzzer
    add_executable(FuzzIPCMessages FuzzIPCMessages.cpp)
    target_link_libraries(FuzzIPCMessages PRIVATE LibIPC LibWebContent)
    target_compile_options(FuzzIPCMessages PRIVATE
        -fsanitize=fuzzer,address,undefined
        -g -O1
    )

    # HTML parser fuzzer
    add_executable(FuzzHTMLParser FuzzHTMLParser.cpp)
    target_link_libraries(FuzzHTMLParser PRIVATE LibWeb)
    target_compile_options(FuzzHTMLParser PRIVATE
        -fsanitize=fuzzer,address,undefined
        -g -O1
    )
endif()
```

### Build Commands
```bash
# Configure with fuzzing enabled
cmake -B Build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DENABLE_FUZZING=ON \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang

# Build fuzz targets
cmake --build Build --target FuzzIPCMessages FuzzHTMLParser
```

## 3. Running Fuzz Campaigns

### LibFuzzer (Fast, Coverage-Guided)
```bash
# Run IPC message fuzzer
./Build/Fuzzing/FuzzIPCMessages \
    -max_len=65536 \
    -timeout=10 \
    -rss_limit_mb=2048 \
    -dict=Fuzzing/ipc_messages.dict \
    Fuzzing/Corpus/ipc/

# Parallel fuzzing (use all cores)
./Build/Fuzzing/FuzzIPCMessages \
    -jobs=8 \
    -workers=8 \
    Fuzzing/Corpus/ipc/
```

### AFL++ (Advanced Techniques)
```bash
# Build with AFL++ instrumentation
export AFL_USE_ASAN=1
export AFL_USE_UBSAN=1

cmake -B Build-AFL \
    -DCMAKE_CXX_COMPILER=afl-clang-fast++ \
    -DCMAKE_C_COMPILER=afl-clang-fast

# Run AFL++
afl-fuzz \
    -i Fuzzing/Corpus/ipc-seed/ \
    -o Fuzzing/Corpus/ipc-findings/ \
    -m none \
    -t 1000 \
    -- ./Build-AFL/Fuzzing/FuzzIPCMessages @@
```

## 4. Crash Triage

### Automated Crash Analysis
```bash
#!/bin/bash
# scripts/triage_crashes.sh

CRASHES_DIR="$1"
OUTPUT_DIR="crash_reports"

mkdir -p "$OUTPUT_DIR"

for crash in "$CRASHES_DIR"/crash-*; do
    echo "Analyzing $crash..."

    # Get stack trace
    ./Build/Fuzzing/FuzzIPCMessages "$crash" 2>&1 | \
        tee "$OUTPUT_DIR/$(basename "$crash").trace"

    # Check exploitability
    if grep -q "SEGV on unknown address" "$OUTPUT_DIR/$(basename "$crash").trace"; then
        echo "⚠️  Potentially exploitable: $crash"
    fi
done
```

### Minimizing Reproducers
```bash
# Minimize crash input
./Build/Fuzzing/FuzzIPCMessages \
    -minimize_crash=1 \
    -max_total_time=60 \
    crash-file

# This produces: minimized-from-crash-file
```

### Converting to Unit Test
```cpp
// Tests/LibIPC/TestIPCSecurity.cpp
TEST_CASE(malformed_message_doesnt_crash)
{
    // From minimized fuzzer input
    u8 const malformed_message[] = {
        0x01, 0x00, 0x00, 0x00,  // type
        0xff, 0xff, 0xff, 0xff   // invalid size
    };

    IPC::Decoder decoder(malformed_message, sizeof(malformed_message));
    auto result = handle_ipc_message(decoder);

    // Should return error, not crash
    EXPECT(result.is_error());
}
```

## 5. Coverage Analysis

### Generate Coverage Report
```bash
# Build with coverage
cmake -B Build-Cov \
    -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Run fuzzer briefly
LLVM_PROFILE_FILE="ipc_fuzzer.profraw" \
    ./Build-Cov/Fuzzing/FuzzIPCMessages \
    -runs=1000000 \
    Fuzzing/Corpus/ipc/

# Generate coverage report
llvm-profdata merge -sparse ipc_fuzzer.profraw -o ipc_fuzzer.profdata
llvm-cov show ./Build-Cov/Fuzzing/FuzzIPCMessages \
    -instr-profile=ipc_fuzzer.profdata \
    -format=html \
    -output-dir=coverage_report

# Open coverage_report/index.html
```

## 6. Corpus Management

### Seed Corpus Creation
```bash
# Create initial corpus
mkdir -p Fuzzing/Corpus/ipc-seed/

# Add valid IPC messages
./scripts/generate_valid_ipc_messages.sh > Fuzzing/Corpus/ipc-seed/

# Add edge cases
echo -n "" > Fuzzing/Corpus/ipc-seed/empty
echo -n "\x00" > Fuzzing/Corpus/ipc-seed/null_byte
python3 -c "print('A' * 65536)" > Fuzzing/Corpus/ipc-seed/max_size
```

### Corpus Minimization
```bash
# Merge and minimize corpus
./Build/Fuzzing/FuzzIPCMessages \
    -merge=1 \
    Fuzzing/Corpus/ipc-minimized/ \
    Fuzzing/Corpus/ipc/ \
    Fuzzing/Corpus/ipc-findings/queue/

# Use minimized corpus for faster runs
```

## 7. Dictionary Files

### IPC Message Dictionary
```
# Fuzzing/ipc_messages.dict
# Message types
"LoadURL"
"SetCookie"
"ExecuteScript"

# Common strings
"http://"
"https://"
"javascript:"
"data:"

# Sizes
"\x00\x00\x00\x00"
"\xff\xff\xff\xff"
"\x00\x00\x10\x00"
```

## 8. Continuous Fuzzing

### CI Integration
```yaml
# .github/workflows/fuzzing.yml
name: Continuous Fuzzing

on:
  schedule:
    - cron: '0 */6 * * *'  # Every 6 hours

jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build fuzzers
        run: |
          cmake -B Build -DENABLE_FUZZING=ON
          cmake --build Build

      - name: Run fuzzing campaign
        run: |
          timeout 3600 ./Build/Fuzzing/FuzzIPCMessages \
            -max_total_time=3600 \
            Fuzzing/Corpus/ipc/

      - name: Upload crashes
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: crashes
          path: crash-*
```

## Checklist

- [ ] Fuzz target compiles without warnings
- [ ] Sanitizers enabled (ASan, UBSan, MSan)
- [ ] Seed corpus created
- [ ] Dictionary file provided
- [ ] Timeout set appropriately
- [ ] Memory limit configured
- [ ] Parallel fuzzing configured
- [ ] Crash triage script ready
- [ ] Coverage tracking enabled
- [ ] CI integration configured

## Common Issues

**Slow fuzzing**: Increase `-jobs` and `-workers`
**OOM crashes**: Reduce `-rss_limit_mb`
**Timeouts**: Increase `-timeout` value
**No coverage**: Check if instrumentation is enabled

## Related Skills

### Complementary Skills
- **[ipc-security](../ipc-security/SKILL.md)**: Combine fuzzing with IPC security validation. Fuzz IPC message handlers to discover vulnerabilities in input validation, rate limiting, and resource management.
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug crashes discovered by fuzzers using ASAN/UBSAN. Analyze crash reports, reproduce issues, and create regression tests for fuzzer-found bugs.

### Implementation Dependencies
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Build fuzzer targets with AFL++ or LibFuzzer instrumentation. Configure CMake with ENABLE_FUZZERS=ON and understand fuzzer target compilation.
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Set up continuous fuzzing in CI pipeline. Run fuzzer corpus regression tests and integrate OSS-Fuzz for long-running fuzzing campaigns.

### Testing Integration
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Generate fuzzer corpus from WPT tests and real-world web pages. Use fuzzing to discover edge cases not covered by standard tests.
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Fuzz IPC boundaries between processes. Test process isolation, sandbox escapes, and crash recovery mechanisms.

### Related Development
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Follow C++ patterns when writing fuzzer harnesses. Use ErrorOr<T> and proper error handling in fuzz targets.
