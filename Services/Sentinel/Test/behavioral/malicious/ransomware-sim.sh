#!/bin/bash
# Malicious test file: Ransomware behavior simulator (SAFE - temp dir only)
# Expected threat score: > 0.7
# WARNING: This simulates ransomware patterns but is confined to temp directory

echo "[Ransomware Simulator] Starting file encryption simulation..."

# Create temporary test directory (sandboxed environment)
TEST_DIR="${TMPDIR:-/tmp}/ransomware_sim_$$"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR" || exit 1

echo "[Ransomware Simulator] Working in: $TEST_DIR"

# Simulate ransomware-like behavior patterns
# Pattern 1: Create multiple files rapidly
echo "[Ransomware Simulator] Creating test files..."
for i in {1..20}; do
    echo "Original content $i" > "file_$i.txt"
done

# Pattern 2: Rename files with suspicious extension
echo "[Ransomware Simulator] Renaming files (encryption simulation)..."
for file in file_*.txt; do
    mv "$file" "$file.encrypted"
done

# Pattern 3: Rapid file modifications (encryption simulation)
echo "[Ransomware Simulator] Simulating file encryption..."
for file in *.encrypted; do
    # Simulate encryption by base64 encoding
    base64 "$file" > "$file.tmp"
    mv "$file.tmp" "$file"
done

# Pattern 4: Create ransom note (common ransomware behavior)
cat > "RANSOM_NOTE.txt" << 'EOF'
!!! YOUR FILES HAVE BEEN ENCRYPTED !!!

This is a SIMULATION for testing purposes only.
No real encryption has occurred.

In real ransomware:
- All your files would be encrypted
- You would need to pay ransom for decryption key
- There is no guarantee of file recovery

Test completed successfully.
EOF

echo "[Ransomware Simulator] Ransom note created"

# Pattern 5: Attempt to access many files (typical ransomware scan)
echo "[Ransomware Simulator] Simulating file system scan..."
find "$TEST_DIR" -type f > /dev/null 2>&1

# Cleanup
echo "[Ransomware Simulator] Cleaning up test directory..."
rm -rf "$TEST_DIR"

echo "[Ransomware Simulator] Simulation complete (all test files removed)"
exit 0
