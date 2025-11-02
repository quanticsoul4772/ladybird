#!/bin/bash
# Benign test file: Simple hello world script
# Expected threat score: < 0.1

echo "Hello from Sentinel BehavioralAnalyzer test!"
echo "Current date: $(date)"
echo "This is a benign script with minimal system calls."

# Simple arithmetic
result=$((2 + 2))
echo "2 + 2 = $result"

exit 0
