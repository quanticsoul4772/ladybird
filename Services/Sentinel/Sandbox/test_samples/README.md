# Sentinel Sandbox Test Dataset

This directory contains a comprehensive test dataset for validating the Sentinel security system's malware detection and sandboxing capabilities. The dataset is organized into malicious and benign samples to enable testing of detection accuracy and false positive rates.

## Directory Structure

```
test_samples/
├── malicious/           # Samples that should trigger detection
│   ├── eicar.txt       # EICAR antivirus test file
│   ├── packed_executable.bin  # High-entropy PE executable
│   ├── suspicious_script.js   # JavaScript with malicious patterns
│   └── README.md        # Detailed malicious sample documentation
├── benign/              # Clean files that should NOT trigger detection
│   ├── hello_world.txt  # Plain text file
│   ├── image.png        # Valid PNG image
│   ├── document.pdf     # Valid PDF document
│   └── README.md        # Detailed benign sample documentation
└── README.md            # This file
```

## Quick Reference

### Malicious Samples
| File | Type | Size | Expected YARA | Expected ML | Threat |
|------|------|------|---------------|-------------|--------|
| eicar.txt | Text | 68 B | >0.5 | N/A | High |
| packed_executable.bin | Binary | 276 B | >0.6 | >7.0 entropy | High |
| suspicious_script.js | Script | 1.2 KB | >0.8 | >0.7 | High |

### Benign Samples
| File | Type | Size | Expected YARA | Expected ML | Threat |
|------|------|------|---------------|-------------|--------|
| hello_world.txt | Text | 54 B | <0.1 | <0.2 | Clean |
| image.png | Image | 70 B | <0.1 | <0.2 | Clean |
| document.pdf | Document | 507 B | <0.1 | <0.2 | Clean |

## Usage

### Manual Testing

Analyze individual files with the Sentinel analyzer:

```bash
# Test a malicious sample
./Build/release/bin/Sentinel --analyze-file test_samples/malicious/eicar.txt

# Test a benign sample
./Build/release/bin/Sentinel --analyze-file test_samples/benign/hello_world.txt
```

### Automated Testing

Run the sandbox test suite:

```bash
# Run all Sentinel tests
./Build/release/bin/TestSandbox

# Run with verbose output
./Build/release/bin/TestSandbox --verbose
```

### Batch Testing

Test all samples in a directory:

```bash
# Test all malicious samples
for file in test_samples/malicious/*; do
    ./Build/release/bin/Sentinel --analyze-file "$file"
done

# Test all benign samples
for file in test_samples/benign/*; do
    ./Build/release/bin/Sentinel --analyze-file "$file"
done
```

## Test Metrics

When running tests, measure:

1. **True Positive Rate (TPR)**: Malicious samples correctly detected
   - Goal: >95% of malicious samples detected

2. **True Negative Rate (TNR)**: Benign samples not falsely flagged
   - Goal: >99% of benign samples pass (false positive <1%)

3. **Detection Consistency**: Same file produces same score across runs
   - Goal: Score variance <1%

4. **Performance**: Analysis time per sample
   - Goal: <100ms per file on typical hardware

## Safety Information

### Important: All Samples Are Safe

- **EICAR file**: Industry-standard antivirus test file, used worldwide
- **Packed executable**: Not executable, binary format only
- **Suspicious script**: Contains undefined functions, will not execute
- **Benign samples**: Legitimate file formats with benign content

**All samples in this directory are completely safe to:**
- Store in version control
- Share with other developers
- Run automated tests on
- Open with standard applications
- Display to users

### No Real Malware

This directory contains NO actual malicious code or executable programs. All samples are designed to:
1. Test detection patterns safely
2. Avoid triggering system security alerts
3. Work across all platforms
4. Enable reliable, repeatable testing

## Adding New Samples

To add new test samples:

1. **Create the file** in the appropriate directory:
   ```bash
   mkdir -p test_samples/malicious  # or benign/
   ```

2. **Add documentation**:
   - Update the README.md in that directory
   - Include expected detection scores
   - Explain the purpose of the test

3. **Update this README.md**:
   - Add entry to Quick Reference table
   - Document new detection patterns if needed

4. **Commit to version control**:
   ```bash
   git add test_samples/
   git commit -m "Sentinel: Add new test samples for [feature]"
   ```

## Expected Behavior

### Malicious Sample Analysis

When analyzing `test_samples/malicious/eicar.txt`:
```
File: eicar.txt
Size: 68 bytes
Detection Results:
  - YARA Rules: MATCHED (EICAR-STANDARD-ANTIVIRUS-TEST-FILE)
  - Machine Learning: N/A (text file)
  - Overall Threat Score: 0.95 (High)
  - Recommendation: Block/Quarantine
```

### Benign Sample Analysis

When analyzing `test_samples/benign/hello_world.txt`:
```
File: hello_world.txt
Size: 54 bytes
Detection Results:
  - YARA Rules: No matches
  - Machine Learning: N/A (text file)
  - Overall Threat Score: 0.0 (Clean)
  - Recommendation: Allow
```

## Integration with CI/CD

These samples can be integrated into continuous testing:

```yaml
# Example GitHub Actions workflow
- name: Run Sentinel Tests
  run: |
    ./Meta/ladybird.py build
    ./Build/release/bin/TestSandbox

- name: Measure Detection Rates
  run: |
    python3 test_samples/measure_detection.py
```

## Related Documentation

- `Services/Sentinel/README.md` - Sentinel system overview
- `docs/SENTINEL_ARCHITECTURE.md` - Detailed system architecture
- `docs/SENTINEL_SETUP_GUIDE.md` - Installation and setup
- `docs/SENTINEL_USER_GUIDE.md` - User guide and features

## Version Control Notes

- All files in this directory are safe to commit
- No .gitignore needed for this directory
- Regular git operations work normally
- No special handling required

## Support

For issues or questions about test samples:
1. Check the sample's README.md in malicious/ or benign/
2. Review the Sentinel documentation
3. Check the Ladybird development guide (CLAUDE.md)
4. File an issue on the project repository

## Changelog

### Version 1.0 (2025-11-01)
- Initial test dataset creation
- 3 malicious samples: EICAR, packed executable, suspicious script
- 3 benign samples: text, PNG image, PDF document
- Comprehensive documentation and usage guides
