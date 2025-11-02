# Sentinel Sandbox Integration Tests

This directory contains end-to-end integration tests for Ladybird's Sentinel security system, specifically testing the download scanning pipeline (Milestone 0.5 Phase 1d).

## Test Files

### download-scanning.html

**Purpose**: Verify the complete download scanning flow from RequestServer → Orchestrator → WasmExecutor → VerdictEngine.

**Test Coverage**:

1. **Clean File Download** - Verifies low-threat files pass through without sandbox execution
2. **Suspicious File Download** - Tests YARA pattern detection and Tier 1 WASM sandbox activation
3. **Malicious File Download (EICAR)** - Tests malware detection with standard EICAR test pattern
4. **Large File Download** - Verifies size-based scanning strategies (streaming, partial scanning)
5. **Binary Executable** - Tests PE header detection and high-priority scanning
6. **Obfuscated JavaScript** - Tests script analysis and obfuscation detection
7. **Archive File** - Tests archive inspection and nested file handling
8. **Office Document** - Tests macro detection and document-specific analysis

## Expected Flow

When a download is triggered via the test:

```
Browser (LibWeb)
    ↓ createElement('a').click()
RequestServer::Request
    ↓ on_data_complete()
SecurityTap::inspect_download()
    ↓ compute_sha256(), check PolicyGraph cache
Orchestrator::analyze_file()
    ├─→ Static Analysis (YARA + ML features)
    ├─→ If suspicious: WasmExecutor::execute_tier1()
    ├─→ If still suspicious: BehavioralAnalyzer::execute_tier2()
    └─→ VerdictEngine::generate_verdict()
        ↓
PolicyGraph::cache_verdict()
    ↓
IPC Alert to WebContent (if threat detected)
    ↓
User notification in browser UI
```

## Architecture Components Tested

### Core Components

- **RequestServer::SecurityTap** - Download interception and SHA256 computation
- **Sentinel::Sandbox::Orchestrator** - Analysis coordination and component lifecycle
- **Sentinel::Sandbox::WasmExecutor** - Tier 1 WASM-based sandbox execution
- **Sentinel::Sandbox::BehavioralAnalyzer** - Tier 2 native behavioral analysis
- **Sentinel::Sandbox::VerdictEngine** - Threat scoring and verdict generation
- **Sentinel::PolicyGraph** - Verdict caching and trusted relationship management

### Integration Points

1. **IPC Communication** - RequestServer ↔ Sentinel daemon
2. **Size-Based Scanning** - ScanSizeConfig determines scan strategy
3. **Graceful Degradation** - System continues if Sentinel unavailable
4. **User Alerts** - Security threats trigger IPC messages to UI

## Running the Test

### Standard Test Runner

```bash
# Run the specific test
./Meta/ladybird.py run test-web -- -f Text/input/sandbox/download-scanning.html

# Run all sandbox tests
./Meta/ladybird.py run test-web -- -f Text/input/sandbox/

# Rebaseline if expected output changes
./Meta/ladybird.py run test-web -- --rebaseline -f Text/input/sandbox/download-scanning.html
```

### Manual Testing

1. Build Ladybird with Sentinel enabled:
   ```bash
   ./Meta/ladybird.py build
   ```

2. Start Sentinel daemon (if not auto-started):
   ```bash
   ./Build/release/bin/Sentinel
   ```

3. Run Ladybird and open the test HTML directly:
   ```bash
   ./Build/release/bin/Ladybird file:///path/to/Tests/LibWeb/Text/input/sandbox/download-scanning.html
   ```

4. Check console output and logs:
   ```bash
   # RequestServer logs (download scanning)
   grep "SecurityTap" /tmp/ladybird-*.log

   # Sentinel logs (sandbox execution)
   grep "Orchestrator\|WasmExecutor\|VerdictEngine" /tmp/sentinel.log
   ```

## Test Limitations

### What This Test DOES

- ✅ Verifies download initiation via JavaScript
- ✅ Tests various file types and threat levels
- ✅ Validates test framework integration
- ✅ Documents expected behavior

### What This Test DOES NOT

- ❌ Directly verify IPC messages (requires process inspection)
- ❌ Check actual YARA rule matches (depends on rule configuration)
- ❌ Validate sandbox execution time/performance
- ❌ Verify quarantine file operations
- ❌ Test user interaction with security alerts

### Why These Limitations?

LibWeb Text tests run in a sandboxed environment with limited access to:
- Inter-process communication internals
- Filesystem operations (quarantine)
- UI components (alert dialogs)
- Sentinel daemon state

For comprehensive testing of these components, see:
- `Services/Sentinel/TestOrchestrator.cpp` - C++ unit tests
- `Tests/Playwright/tests/security/malware-detection.spec.ts` - End-to-end browser tests

## Interpreting Results

### Success Criteria

The test **passes** if:
1. All 8 download scenarios complete without JavaScript errors
2. Console output matches expected output exactly
3. No uncaught exceptions during blob creation or URL manipulation

### Expected Sentinel Behavior

When running with Sentinel enabled, you should see in logs:

```
SecurityTap: Connected to Sentinel daemon
SecurityTap: Inspecting download: clean.txt (SHA256: ...)
Orchestrator: Analyzing file clean.txt (52 bytes)
VerdictEngine: Clean file (score: 0.05)

SecurityTap: Inspecting download: eicar.com (SHA256: ...)
Orchestrator: Analyzing file eicar.com (68 bytes)
WasmExecutor: Tier 1 execution started
VerdictEngine: Malicious file detected (score: 0.95)
PolicyGraph: Caching verdict for eicar.com
```

### Graceful Degradation

If Sentinel is **not running**, RequestServer should log:
```
SecurityTap: Failed to connect to Sentinel daemon
SecurityTap: Continuing without malware scanning (fail-open mode)
```

Downloads proceed normally without security scanning.

## Test Data Details

### EICAR Test Virus

The test uses the **EICAR standard anti-virus test file**:
- Standard, safe-to-use malware signature
- Recognized by all major anti-virus software
- Should trigger YARA rule: `EICAR_Test_File`
- More info: https://www.eicar.org/

### File Signatures

| File Type | Signature | Bytes |
|-----------|-----------|-------|
| PE Executable | `4D 5A` | MZ |
| ZIP Archive | `50 4B 03 04` | PK |
| OLE Document | `D0 CF 11 E0 A1 B1 1A E1` | OLE |

These signatures trigger file-type-specific YARA rules and analysis strategies.

## Troubleshooting

### Test Fails with "Uncaught Error"

**Cause**: JavaScript error during blob/URL creation
**Fix**: Check browser console for specific error message

### Downloads Not Scanned

**Cause**: Sentinel daemon not running or SecurityTap disabled
**Fix**: Start Sentinel or check RequestServer connection logs

### Expected Output Mismatch

**Cause**: Console output differs from expected
**Fix**: Run with `--rebaseline` to update expected output

### No Quarantine Actions

**Cause**: LibWeb Text tests don't have filesystem access
**Fix**: This is expected; use C++ unit tests or Playwright tests for file operations

## Related Documentation

- `docs/SANDBOX_ARCHITECTURE.md` - Sentinel sandbox system design
- `docs/MILESTONE_0.5_PHASE_1D.md` - Phase 1d implementation details
- `Services/Sentinel/Sandbox/README.md` - Orchestrator component docs
- `Services/RequestServer/README.md` - SecurityTap integration

## Future Enhancements

Potential additions to this test suite:

1. **Fetch API Integration** - Test downloads via `fetch()` instead of `<a>` tags
2. **XMLHttpRequest** - Test legacy AJAX download patterns
3. **Service Worker Interception** - Test downloads through service workers
4. **Blob URL Lifetime** - Test verdict caching across URL revocation
5. **Concurrent Downloads** - Test parallel download scanning
6. **Memory Limits** - Test handling of extremely large files

## Contributing

When adding new sandbox tests:

1. Follow the existing pattern: use `test()` with `println()` output
2. Create both `.html` input and `.txt` expected output
3. Document test scenarios in this README
4. Add C++ unit tests for low-level component testing
5. Consider adding Playwright tests for UI interactions
