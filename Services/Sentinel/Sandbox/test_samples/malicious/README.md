# Malicious Test Samples

These are SAFE test samples designed to trigger malware detection without being actual malware. They are intended for testing the Sentinel security system's detection capabilities.

## Samples

### 1. eicar.txt
- **Type**: Standard EICAR anti-virus test file
- **Content**: The standard EICAR test string
- **Size**: 68 bytes
- **Purpose**: Universal antivirus test file recognized by all major security vendors
- **Safety**: Completely safe - this is an industry-standard test file
- **Expected Detection**:
  - YARA score: >0.5 (pattern matches)
  - Threat level: High
  - Detection: EICAR-STANDARD-ANTIVIRUS-TEST-FILE pattern

### 2. packed_executable.bin
- **Type**: High-entropy simulated packed executable
- **Content**: PE header (MZ) + high-entropy random data
- **Size**: 276 bytes
- **Purpose**: Tests entropy-based detection for packed/compressed executables
- **Safety**: Not executable - binary format only
- **Expected Detection**:
  - YARA score: >0.6 (PE header detection)
  - ML entropy score: >7.0 (high entropy indicates packing/compression)
  - Threat level: High
  - Detection: Packed executable signature

### 3. suspicious_script.js
- **Type**: JavaScript with multiple malicious patterns
- **Content**: Code containing suspicious function calls and patterns
- **Size**: ~1.2 KB
- **Purpose**: Tests pattern-based detection for malicious scripts
- **Safety**: Syntactically valid but contains undefined functions that would not execute
- **Expected Detection**:
  - YARA score: >0.8 (many suspicious patterns)
  - Threat level: High
  - Detection: Multiple suspicious patterns:
    - Shell command execution (cmd.exe, powershell)
    - eval() usage
    - Registry manipulation
    - Credential stealing patterns
    - Remote code execution patterns

## Notes

- All samples are safe to store in version control
- The EICAR file is recognized worldwide as a safe test file
- The JavaScript file won't execute due to undefined global functions
- Binary file is not executable on any platform
- These files are suitable for automated testing without human review

## Related Files

See the parent README.md for usage instructions and the benign/ directory for clean comparison samples.
