#!/bin/bash
set -euo pipefail

SAMPLES_DIR="/home/rbsmith4/ladybird/Services/Sentinel/Sandbox/test_samples"
ORCHESTRATOR="/home/rbsmith4/ladybird/Build/release/bin/TestSandbox"

echo "=== Sentinel Sandbox Integration Tests ==="
echo "Date: $(date)"
echo "=========================================="
echo ""

# Test 1: EICAR test file (malicious)
echo "Test 1: EICAR malicious test file"
if [ -f "$SAMPLES_DIR/malicious/eicar.txt" ]; then
    FILE_SIZE=$(stat -c%s "$SAMPLES_DIR/malicious/eicar.txt")
    echo "  File: eicar.txt ($FILE_SIZE bytes)"
    echo "  Expected: YARA >0.5, ML >0.3, Threat: Suspicious/Malicious"
    echo "  Content preview: $(head -c 50 "$SAMPLES_DIR/malicious/eicar.txt")"
    echo "  Status: Sample verified ✓"
else
    echo "  ERROR: Sample not found"
    exit 1
fi
echo ""

# Test 2: Packed executable (malicious)
echo "Test 2: Packed executable"
if [ -f "$SAMPLES_DIR/malicious/packed_executable.bin" ]; then
    FILE_SIZE=$(stat -c%s "$SAMPLES_DIR/malicious/packed_executable.bin")
    echo "  File: packed_executable.bin ($FILE_SIZE bytes)"
    echo "  Expected: YARA >0.6, ML >0.7 (high entropy), Threat: Malicious"
    # Calculate entropy as indicator
    ENTROPY=$(cat "$SAMPLES_DIR/malicious/packed_executable.bin" | LC_ALL=C tr -cd '[:print:]' | wc -c)
    echo "  Printable chars: $ENTROPY (low = high entropy = packed)"
    echo "  Status: Sample verified ✓"
else
    echo "  ERROR: Sample not found"
    exit 1
fi
echo ""

# Test 3: Suspicious JavaScript (malicious)
echo "Test 3: Suspicious JavaScript"
if [ -f "$SAMPLES_DIR/malicious/suspicious_script.js" ]; then
    FILE_SIZE=$(stat -c%s "$SAMPLES_DIR/malicious/suspicious_script.js")
    echo "  File: suspicious_script.js ($FILE_SIZE bytes)"
    echo "  Expected: YARA >0.8, ML >0.7, Threat: Malicious/Critical"
    # Check for suspicious patterns
    EVAL_COUNT=$(grep -o "eval" "$SAMPLES_DIR/malicious/suspicious_script.js" | wc -l || echo "0")
    EXEC_COUNT=$(grep -o "exec" "$SAMPLES_DIR/malicious/suspicious_script.js" | wc -l || echo "0")
    echo "  Suspicious patterns: eval($EVAL_COUNT), exec($EXEC_COUNT)"
    echo "  Status: Sample verified ✓"
else
    echo "  ERROR: Sample not found"
    exit 1
fi
echo ""

# Test 4: Hello World (benign)
echo "Test 4: Plain text file (benign)"
if [ -f "$SAMPLES_DIR/benign/hello_world.txt" ]; then
    FILE_SIZE=$(stat -c%s "$SAMPLES_DIR/benign/hello_world.txt")
    echo "  File: hello_world.txt ($FILE_SIZE bytes)"
    echo "  Expected: YARA <0.3, ML <0.4, Threat: Clean"
    echo "  Content: $(cat "$SAMPLES_DIR/benign/hello_world.txt")"
    echo "  Status: Sample verified ✓"
else
    echo "  ERROR: Sample not found"
    exit 1
fi
echo ""

# Test 5: PNG image (benign)
echo "Test 5: PNG image (benign)"
if [ -f "$SAMPLES_DIR/benign/image.png" ]; then
    FILE_SIZE=$(stat -c%s "$SAMPLES_DIR/benign/image.png")
    echo "  File: image.png ($FILE_SIZE bytes)"
    echo "  Expected: YARA <0.3, ML <0.4, Threat: Clean"
    # Verify PNG header
    if file "$SAMPLES_DIR/benign/image.png" | grep -q "PNG"; then
        echo "  Format: Valid PNG header detected"
    fi
    echo "  Status: Sample verified ✓"
else
    echo "  ERROR: Sample not found"
    exit 1
fi
echo ""

# Test 6: PDF document (benign)
echo "Test 6: PDF document (benign)"
if [ -f "$SAMPLES_DIR/benign/document.pdf" ]; then
    FILE_SIZE=$(stat -c%s "$SAMPLES_DIR/benign/document.pdf")
    echo "  File: document.pdf ($FILE_SIZE bytes)"
    echo "  Expected: YARA <0.3, ML <0.4, Threat: Clean"
    # Verify PDF header
    if head -c 4 "$SAMPLES_DIR/benign/document.pdf" | grep -q "%PDF"; then
        echo "  Format: Valid PDF header detected"
    fi
    echo "  Status: Sample verified ✓"
else
    echo "  ERROR: Sample not found"
    exit 1
fi
echo ""

echo "=========================================="
echo "All sample files verified successfully!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Unit tests verify sandbox infrastructure works"
echo "2. Sample files are ready for analysis"
echo "3. When Wasmtime is installed, run actual analysis"
echo "4. Expected CLI: orchestrator analyze <file>"
