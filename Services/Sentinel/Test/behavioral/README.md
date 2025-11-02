# Behavioral Analyzer Test Dataset

This directory contains test files for validating the BehavioralAnalyzer component.

## Directory Structure

```
behavioral/
├── benign/          # Files with benign behavior patterns
└── malicious/       # Files with malicious behavior patterns
```

## Benign Test Files

### hello.sh
- **Purpose**: Simple shell script that prints a message
- **Expected Behavior**: Minimal system calls, no network activity
- **Expected Threat Score**: < 0.1

### calculator.py
- **Purpose**: Simple Python calculator script
- **Expected Behavior**: Basic arithmetic operations, no file I/O
- **Expected Threat Score**: < 0.2

## Malicious Test Files

### eicar.txt
- **Purpose**: EICAR anti-malware test file (industry standard)
- **Expected Behavior**: Should be detected by YARA rules
- **Expected Threat Score**: > 0.9
- **Note**: Harmless test file, safe to handle

### ransomware-sim.sh
- **Purpose**: Simulates ransomware-like file operations (safe)
- **Expected Behavior**:
  - Creates/renames multiple files
  - File encryption patterns
  - Attempts to modify many files
- **Expected Threat Score**: > 0.7
- **Note**: Operates only in temp directory, safe to execute

## Test Score Thresholds

Based on `docs/BEHAVIORAL_ANALYSIS_SPEC.md`:

| Score Range | Threat Level | Action |
|-------------|--------------|--------|
| 0.0 - 0.3   | Low          | Allow  |
| 0.3 - 0.7   | Medium       | Warn   |
| 0.7 - 1.0   | High         | Block  |

## Adding New Test Files

When adding new test files:
1. Place in appropriate directory (benign/ or malicious/)
2. Document expected behavior and threat score
3. Update this README
4. Add corresponding test case in `TestBehavioralAnalyzer.cpp`

## Safety Notes

- All test files are safe to execute in sandboxed environment
- Malicious samples are simulations, not real malware
- Always run tests in isolated environment (nsjail sandbox)
- Do not execute malicious samples outside of test framework
