# Sandbox Integration Test - Quick Start

**Test**: Download scanning end-to-end integration
**Location**: `Tests/LibWeb/Text/input/sandbox/download-scanning.html`
**Time**: < 1 second
**Prerequisites**: None (works with/without Sentinel)

---

## Run the Test

```bash
# Standard test runner
./Meta/ladybird.py run test-web -- -f Text/input/sandbox/download-scanning.html

# With verification and log analysis
./Tests/LibWeb/Text/input/sandbox/verify-test.sh

# Manual browser testing
./Build/release/bin/Ladybird file://$(pwd)/Tests/LibWeb/Text/input/sandbox/download-scanning.html
```

---

## What It Tests

8 download scenarios covering the full Sentinel pipeline:

1. **Clean file** → Low score, no sandbox
2. **Suspicious file** → Medium score, Tier 1 WASM sandbox
3. **EICAR malware** → High score, full analysis + quarantine
4. **Large file (1MB)** → Size-based scanning strategy
5. **PE executable** → Binary analysis, signature detection
6. **Obfuscated JS** → Script deobfuscation, behavior analysis
7. **ZIP archive** → Archive inspection, nested files
8. **Office doc** → Macro detection, VBA analysis

---

## Expected Flow

```
Download → SecurityTap → Orchestrator → [YARA + ML + Sandbox] → VerdictEngine → PolicyGraph → Alert
```

---

## Success Criteria

✅ All 8 scenarios show "✓" (no "✗ Error")
✅ Output matches `expected/sandbox/download-scanning.txt`
✅ No JavaScript exceptions

---

## Debugging

**If test fails**:
1. Check browser console for JavaScript errors
2. Run with Sentinel daemon: `./Build/release/bin/Sentinel`
3. Check logs: `grep -i "securitytap\|orchestrator" /tmp/*.log`

**If downloads not scanned**:
- Expected if Sentinel not running (fail-open mode)
- Start Sentinel daemon and re-run test
- Check `/tmp/sentinel.sock` exists

---

## Documentation

- **Full guide**: `Tests/LibWeb/Text/input/sandbox/README.md`
- **Coverage details**: `Tests/LibWeb/Text/input/sandbox/TEST_DELIVERABLE.md`
- **Project summary**: `/home/rbsmith4/ladybird/SANDBOX_INTEGRATION_TEST_SUMMARY.md`

---

## Related Tests

- **C++ unit**: `./Build/release/bin/TestOrchestrator`
- **Playwright E2E**: `npm test --prefix Tests/Playwright tests/security/malware-detection.spec.ts`

---

**Status**: ✅ Production-ready
**Milestone**: 0.5 Phase 1d
**Updated**: 2025-11-02
