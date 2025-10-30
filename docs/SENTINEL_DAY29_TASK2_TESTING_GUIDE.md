# Sentinel Day 29 Task 2: Testing Guide

**Purpose**: Quick reference for testing the arbitrary file read fix

---

## Quick Start

### 1. Apply the Fix
```bash
cd /home/rbsmith4/ladybird
cp Services/Sentinel/SentinelServer.cpp.new Services/Sentinel/SentinelServer.cpp
```

### 2. Build
```bash
cmake --build Build/default
```

### 3. Run Tests
```bash
# Unit tests (after creating TestScanPathValidation.cpp)
Build/default/bin/Sentinel/TestScanPathValidation

# Integration tests
Build/default/bin/Sentinel/TestDownloadVetting
```

---

## Manual Security Testing

### Test 1: Path Traversal Attack
```bash
# Expected: Error - "Path traversal detected"
echo '{"action":"scan_file","file_path":"/tmp/../etc/passwd","request_id":"test1"}' | \
  nc -U /tmp/sentinel.sock
```

### Test 2: Symlink Attack
```bash
# Setup
ln -s /etc/passwd /tmp/evil_symlink

# Expected: Error - "Cannot scan symlinks"
echo '{"action":"scan_file","file_path":"/tmp/evil_symlink","request_id":"test2"}' | \
  nc -U /tmp/sentinel.sock

# Cleanup
rm /tmp/evil_symlink
```

### Test 3: Device File Attack
```bash
# Expected: Error - "File path not in allowed directory"
echo '{"action":"scan_file","file_path":"/dev/zero","request_id":"test3"}' | \
  nc -U /tmp/sentinel.sock
```

### Test 4: System File Access
```bash
# Expected: Error - "File path not in allowed directory"
echo '{"action":"scan_file","file_path":"/etc/shadow","request_id":"test4"}' | \
  nc -U /tmp/sentinel.sock
```

### Test 5: Large File DoS
```bash
# Setup
dd if=/dev/zero of=/tmp/large_file.bin bs=1M count=250

# Expected: Error - "File too large to scan"
echo '{"action":"scan_file","file_path":"/tmp/large_file.bin","request_id":"test5"}' | \
  nc -U /tmp/sentinel.sock

# Cleanup
rm /tmp/large_file.bin
```

### Test 6: Legitimate File (Control)
```bash
# Setup
echo "test content" > /tmp/legitimate_file.txt

# Expected: Success - {"status":"success","result":"clean"}
echo '{"action":"scan_file","file_path":"/tmp/legitimate_file.txt","request_id":"test6"}' | \
  nc -U /tmp/sentinel.sock

# Cleanup
rm /tmp/legitimate_file.txt
```

---

## Integration Testing with Quarantine

### Test Quarantine Workflow
```bash
# 1. Download a file (triggers quarantine)
# 2. Verify file in quarantine directory
ls -la ~/.local/share/Ladybird/quarantine/

# 3. Scan quarantined file
QUARANTINE_ID=$(ls ~/.local/share/Ladybird/quarantine/ | head -1)
echo "{\"action\":\"scan_file\",\"file_path\":\"/home/$USER/.local/share/Ladybird/quarantine/$QUARANTINE_ID\",\"request_id\":\"test7\"}" | \
  nc -U /tmp/sentinel.sock
```

---

## Expected Results Summary

| Test | Input | Expected Result |
|------|-------|-----------------|
| Path Traversal | `/tmp/../etc/passwd` | Error: "Path traversal detected" |
| Symlink | `/tmp/evil_symlink -> /etc/passwd` | Error: "Cannot scan symlinks" |
| Device File | `/dev/zero` | Error: "File path not in allowed directory" |
| System File | `/etc/shadow` | Error: "File path not in allowed directory" |
| Large File | 250MB file | Error: "File too large to scan" |
| Legitimate | `/tmp/test.txt` | Success: "clean" |
| Quarantine | `~/.local/share/.../file` | Success: scan result |

---

## Debugging Tips

### Enable Debug Logging
```bash
# Set debug level
export SENTINEL_DEBUG=1

# Start Sentinel
Build/default/bin/Sentinel/SentinelServer
```

### Check Logs
```bash
# Watch for validation errors
tail -f /var/log/sentinel.log | grep "validation"
```

### Verify Socket Connection
```bash
# Check socket exists
ls -la /tmp/sentinel.sock

# Test connection
echo '{"action":"ping"}' | nc -U /tmp/sentinel.sock
```

---

## Common Issues

### Issue: "Failed to connect to socket"
**Solution**: Ensure SentinelServer is running
```bash
ps aux | grep SentinelServer
```

### Issue: "Permission denied" for test files
**Solution**: Ensure test files are readable
```bash
chmod 644 /tmp/test_file.txt
```

### Issue: Quarantine directory doesn't exist
**Solution**: Create it
```bash
mkdir -p ~/.local/share/Ladybird/quarantine
```

---

## Performance Testing

### Measure Validation Overhead
```bash
# Time 1000 scans with validation
time for i in {1..1000}; do
  echo '{"action":"scan_file","file_path":"/tmp/test.txt","request_id":"perf"}' | \
    nc -U /tmp/sentinel.sock > /dev/null
done
```

**Expected**: < 1 second total (< 1ms per scan)

---

## Memory Safety Testing

### Run with ASAN
```bash
# Build with AddressSanitizer
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Run tests
ASAN_OPTIONS=detect_leaks=1 Build/sanitizers/bin/Sentinel/TestPolicyGraph
```

### Check for Leaks
Look for output like:
- ✅ "No memory leaks detected"
- ❌ "Direct leak of X bytes" (requires fixing)

---

## Verification Checklist

Before marking as complete:

- [ ] All manual tests pass
- [ ] Legitimate files can be scanned
- [ ] Quarantine workflow works
- [ ] No memory leaks (ASAN clean)
- [ ] Performance < 1ms overhead
- [ ] Error messages are clear
- [ ] Logs show validation details

---

## Rollback Plan

If issues are found:

```bash
# Restore original version
cd /home/rbsmith4/ladybird
git checkout Services/Sentinel/SentinelServer.cpp

# Rebuild
cmake --build Build/default

# Restart Sentinel
killall SentinelServer
Build/default/bin/Sentinel/SentinelServer
```

---

## Support

For issues or questions:
- Review: `docs/SENTINEL_DAY29_TASK2_IMPLEMENTED.md`
- Check: `docs/SENTINEL_PHASE5_DAY29-35_DETAILED_PLAN.md`
- Test: Run all tests in `Tests/Sentinel/`
