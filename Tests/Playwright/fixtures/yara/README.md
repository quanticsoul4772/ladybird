# YARA Test Rules for Sentinel

This directory contains YARA rules for testing Ladybird's malware detection system.

## Rules Overview

| Rule Name | Purpose | Test File | Severity |
|-----------|---------|-----------|----------|
| `EICAR_Test_File` | Industry-standard AV test | `eicar.txt` | test |
| `Detect_Test_String` | Custom rule verification | `custom-test.txt` | medium |
| `PE_Suspicious_APIs` | Windows malware APIs | `ml-suspicious.bin` | high |
| `High_Entropy_Packed` | Packed executables | (future) | medium |
| `Embedded_URLs` | C2 server URLs | `ml-suspicious.bin` | medium |
| `Suspicious_PowerShell` | PowerShell attacks | `ml-suspicious.bin` | high |
| `Shellcode_Patterns` | Exploit code | (future) | high |
| `Bitcoin_Miner_Strings` | Cryptocurrency miners | (future) | medium |
| `Ransomware_Indicators` | Ransomware detection | (future) | critical |

## Installation

### For Sentinel Service

1. Copy rules to Sentinel's rules directory:
   ```bash
   sudo cp test-rules.yar /etc/sentinel/rules/
   ```

2. Reload Sentinel to pick up new rules:
   ```bash
   sudo systemctl reload sentinel
   # OR
   sudo killall -HUP sentinel
   ```

3. Verify rules loaded:
   ```bash
   sudo journalctl -u sentinel | grep "Loaded.*YARA rules"
   ```

### For Testing

The rules are automatically available in `fixtures/yara/test-rules.yar` for test execution.

## Testing YARA Rules

### MAL-001: EICAR Test File
```bash
npx playwright test malware-detection.spec.ts -g "MAL-001"
```
Expected: `EICAR_Test_File` rule matches, download blocked.

### MAL-006: Custom YARA Rules
```bash
npx playwright test malware-detection.spec.ts -g "MAL-006"
```
Expected: `Detect_Test_String` rule matches custom signature.

### MAL-005: ML Detection (with YARA fallback)
```bash
npx playwright test malware-detection.spec.ts -g "MAL-005"
```
Expected: Multiple rules match (`PE_Suspicious_APIs`, `Embedded_URLs`, `Suspicious_PowerShell`).

## Rule Syntax

YARA rules follow this structure:

```yara
rule RuleName {
    meta:
        description = "What this rule detects"
        author = "Author name"
        severity = "low|medium|high|critical"
        date = "YYYY-MM-DD"

    strings:
        $string1 = "text pattern" ascii
        $hex1 = { 4D 5A 90 00 }
        $regex1 = /pattern[0-9]+/ nocase

    condition:
        $string1 or $hex1
}
```

## False Positive Testing

Verify clean files don't match:
```bash
# Should NOT match any rules
yara test-rules.yar ../malware/clean-file.txt
yara test-rules.yar ../malware/clean-file2.txt

# Should match EICAR rule only
yara test-rules.yar ../malware/eicar.txt

# Should match multiple rules
yara test-rules.yar ../malware/ml-suspicious.bin
```

## Adding Custom Rules

1. Create new `.yar` file in this directory
2. Follow naming convention: `<category>-rules.yar`
3. Include `meta` section with description, severity
4. Test locally before deployment:
   ```bash
   yara -w your-rules.yar test-file.bin
   ```
5. Copy to Sentinel rules directory
6. Reload Sentinel service

## Security Considerations

⚠️ **IMPORTANT**: These rules are for TESTING ONLY.

- Use safe test strings (EICAR) rather than real malware
- Don't distribute files that match production malware signatures
- Rules are intentionally simple for testing clarity
- Production rules should be more sophisticated and regularly updated

## Resources

- [YARA Documentation](https://yara.readthedocs.io/)
- [EICAR Test File](https://www.eicar.org/download-anti-malware-testfile/)
- [YARA Rule Writing Guide](https://yara.readthedocs.io/en/stable/writingrules.html)
- [Awesome YARA](https://github.com/InQuest/awesome-yara)

## Troubleshooting

### Rules Not Loading

Check Sentinel logs:
```bash
sudo journalctl -u sentinel -n 50
```

Common issues:
- Syntax error in rule file
- File permissions (rules must be readable by Sentinel user)
- Sentinel not configured to scan rules directory

### Rules Not Matching

Test manually with YARA CLI:
```bash
yara -s test-rules.yar ../malware/eicar.txt
```

If CLI matches but Sentinel doesn't:
- Verify Sentinel using correct rules directory
- Check Sentinel's YARA library version
- Ensure SecurityTap sending complete file content

## License

These test rules are provided for educational and testing purposes.
Use at your own risk. Not for production malware detection.
