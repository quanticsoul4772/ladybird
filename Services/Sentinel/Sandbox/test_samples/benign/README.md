# Benign Test Samples

Clean files that should NOT trigger malware detection. These files are used to test false positive rates and to verify that the Sentinel security system correctly identifies legitimate content.

## Samples

### 1. hello_world.txt
- **Type**: Plain text file
- **Content**: Simple UTF-8 text message
- **Size**: 54 bytes
- **Purpose**: Tests text file handling and baseline detection
- **Expected Detection**:
  - YARA score: <0.1 (no suspicious patterns)
  - ML score: <0.2 (benign content)
  - Threat level: Clean
  - Detection: No threats

### 2. image.png
- **Type**: PNG image file
- **Content**: Valid 1x1 pixel PNG image with proper headers and structure
- **Size**: 70 bytes
- **Purpose**: Tests binary file handling and image format detection
- **Expected Detection**:
  - YARA score: <0.1 (valid image format)
  - ML score: <0.2 (benign binary content)
  - Threat level: Clean
  - Detection: No threats
  - Format validation: Valid PNG

### 3. document.pdf
- **Type**: PDF document
- **Content**: Minimal but valid PDF file with text content
- **Size**: 507 bytes
- **Purpose**: Tests PDF handling and document format detection
- **Expected Detection**:
  - YARA score: <0.1 (valid PDF format)
  - ML score: <0.2 (no embedded malicious code)
  - Threat level: Clean
  - Detection: No threats
  - Format validation: Valid PDF 1.4

## Notes

- All samples have valid file format headers
- PNG and PDF files are syntactically valid
- These files represent commonly encountered legitimate file types
- They can be safely opened by standard applications
- Suitable for baseline testing and false positive measurement

## Usage in Testing

Compare detection results for these benign samples with the malicious samples in the `malicious/` directory to measure:
1. True positive rate (malicious correctly identified as malicious)
2. False positive rate (benign incorrectly identified as malicious)
3. Detection consistency across file types

## Related Files

See the parent README.md for usage instructions and the malicious/ directory for test samples designed to trigger detection.
