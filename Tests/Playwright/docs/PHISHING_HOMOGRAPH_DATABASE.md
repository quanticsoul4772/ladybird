# Phishing Homograph Attack Database

## Overview

This document catalogs Unicode confusable characters used in homograph phishing attacks. The PhishingURLAnalyzer uses ICU (International Components for Unicode) to detect these attacks.

## What is a Homograph Attack?

A **homograph attack** (also called IDN homograph attack or script spoofing) exploits the visual similarity between Unicode characters from different scripts to create deceptive domain names.

**Example:**
- Legitimate: `apple.com` (Latin 'a')
- Phishing: `аpple.com` (Cyrillic 'а' - U+0430)

To users, these appear identical, but they are different domains.

## Detection Method

Sentinel's PhishingURLAnalyzer uses the **ICU Spoofchecker** library:

1. **Confusable Detection**: `uspoof_checkUTF8()` with `USPOOF_CONFUSABLE` flag
2. **Skeleton Normalization**: `uspoof_getSkeletonUTF8()` converts confusables to ASCII
3. **Visual Similarity**: Compares normalized skeleton to popular domains

**Detection Score**: +0.3 (30% phishing score) when homograph attack detected

## Common Confusable Characters

### Latin ↔ Cyrillic

| Latin | Unicode | Cyrillic | Unicode | Example Attack |
|-------|---------|----------|---------|----------------|
| a | U+0061 | а | U+0430 | аpple.com → apple.com |
| e | U+0065 | е | U+0435 | googlе.com → google.com |
| o | U+006F | о | U+043E | micrоsoft.com → microsoft.com |
| p | U+0070 | р | U+0440 | рaypal.com → paypal.com |
| c | U+0063 | с | U+0441 | faсebook.com → facebook.com |
| y | U+0079 | у | U+0443 | ebау.com → ebay.com |
| x | U+0078 | х | U+0445 | boх.com → box.com |
| s | U+0073 | ѕ | U+0455 | аmazonѕ.com → amazons.com |
| i | U+0069 | і | U+0456 | twіtter.com → twitter.com |
| j | U+006A | ј | U+0458 | јira.com → jira.com |
| B | U+0042 | В | U+0412 | ВMW.com → BMW.com |
| H | U+0048 | Н | U+041D | НTTP.com → HTTP.com |
| P | U+0050 | Р | U+0420 | РAYPAL.com → PAYPAL.com |
| T | U+0054 | Т | U+0422 | ТЕSLA.com → TESLA.com |

### Latin ↔ Greek

| Latin | Unicode | Greek | Unicode | Example Attack |
|-------|---------|-------|---------|----------------|
| o | U+006F | ο | U+03BF | gοοgle.com → google.com |
| a | U+0061 | α | U+03B1 | αmazon.com → amazon.com |
| v | U+0076 | ν | U+03BD | νisa.com → visa.com |
| i | U+0069 | ι | U+03B9 | ιntel.com → intel.com |
| n | U+006E | η | U+03B7 | ηetflix.com → netflix.com |
| p | U+0070 | ρ | U+03C1 | ρinterest.com → pinterest.com |
| y | U+0079 | γ | U+03B3 | γahoo.com → yahoo.com |
| u | U+0075 | υ | U+03C5 | υber.com → uber.com |
| x | U+0078 | χ | U+03C7 | χbox.com → xbox.com |

### Latin ↔ Hebrew

| Latin | Unicode | Hebrew | Unicode | Example Attack |
|-------|---------|--------|---------|----------------|
| o | U+006F | ס | U+05E1 | סracle.com → oracle.com |
| n | U+006E | ո | U+0578 | ոvidia.com → nvidia.com |

### Numbers and Special Characters

| Latin | Unicode | Confusable | Unicode | Example Attack |
|-------|---------|------------|---------|----------------|
| 0 (zero) | U+0030 | О (Cyrillic O) | U+041E | g00gle.com → google.com |
| 1 (one) | U+0031 | l (lowercase L) | U+006C | goog1e.com → google.com |
| 1 (one) | U+0031 | І (Cyrillic I) | U+0406 | goog1e.com → google.com |

## Real-World Phishing Examples

### Historical Attacks

1. **Epic Games Phishing (2018)**
   - Attack: `epicgаmes.com` (Cyrillic 'а')
   - Target: `epicgames.com`
   - Impact: Credential theft via fake login page

2. **Apple Phishing (2017)**
   - Attack: `аpple.com` (Cyrillic 'а')
   - Target: `apple.com`
   - Impact: Punycode `xn--pple-43d.com`

3. **PayPal Phishing (Ongoing)**
   - Attack: `рaypal.com` (Cyrillic 'р')
   - Target: `paypal.com`
   - Impact: Financial credential harvesting

## Test Cases for PhishingURLAnalyzer

### High-Priority Test Domains

```javascript
const HOMOGRAPH_TEST_CASES = [
  // Cyrillic attacks
  { attack: 'аpple.com', target: 'apple.com', char: 'а (U+0430)' },
  { attack: 'gοοgle.com', target: 'google.com', char: 'ο (U+03BF)' },
  { attack: 'рaypal.com', target: 'paypal.com', char: 'р (U+0440)' },
  { attack: 'аmazon.com', target: 'amazon.com', char: 'а (U+0430)' },
  { attack: 'micrοsoft.com', target: 'microsoft.com', char: 'ο (U+03BF)' },
  { attack: 'netfliх.com', target: 'netflix.com', char: 'х (U+0445)' },
  { attack: 'faсebook.com', target: 'facebook.com', char: 'с (U+0441)' },
  { attack: 'instаgram.com', target: 'instagram.com', char: 'а (U+0430)' },
  { attack: 'twіtter.com', target: 'twitter.com', char: 'і (U+0456)' },
  { attack: 'linкedin.com', target: 'linkedin.com', char: 'к (U+043A)' },

  // Multi-character homographs
  { attack: 'gооgle.com', target: 'google.com', char: 'о (U+043E) x2' },
  { attack: 'аррle.com', target: 'apple.com', char: 'а (U+0430), р (U+0440) x2' },

  // Mixed scripts
  { attack: 'раураl.com', target: 'paypal.com', char: 'р (U+0440), у (U+0443), а (U+0430)' }
];
```

### Punycode Representations

| Domain | Punycode | Detection |
|--------|----------|-----------|
| аpple.com | xn--pple-43d.com | ✅ Detected |
| gοοgle.com | xn--ggle-0nda.com | ✅ Detected |
| рaypal.com | xn--aypal-r7e.com | ✅ Detected |
| micrοsoft.com | xn--micrsoft-m7g.com | ✅ Detected |

## ICU Spoofchecker Configuration

### Ladybird Implementation

Located in: `Services/Sentinel/PhishingURLAnalyzer.cpp`

```cpp
// Create spoofchecker
USpoofChecker* checker = uspoof_open(&status);

// Configure for confusable detection
uspoof_setChecks(checker, USPOOF_CONFUSABLE, &status);

// Check domain
int32_t result = uspoof_checkUTF8(checker, domain.data(), domain.length(), nullptr, &status);

// result > 0 means confusables detected
```

### Detection Flags

- `USPOOF_CONFUSABLE`: Detect visually confusable characters
- `USPOOF_SINGLE_SCRIPT_CONFUSABLE`: Within same script only
- `USPOOF_MIXED_SCRIPT_CONFUSABLE`: Across different scripts
- `USPOOF_WHOLE_SCRIPT_CONFUSABLE`: Entire domain confusable

**Current Configuration**: `USPOOF_CONFUSABLE` (detects all types)

## Legitimate IDN vs Homograph Attack

### Legitimate IDN Characteristics

✅ **Should NOT trigger alert:**
- All characters from same script (e.g., all Cyrillic: `президент.рф`)
- Appropriate for language/region (e.g., German umlaut: `münchen.de`)
- No visual similarity to popular Latin domains

❌ **Should trigger alert (homograph):**
- Mixed scripts (e.g., Latin + Cyrillic: `аpple.com`)
- Visually identical to popular domain
- Punycode that normalizes to known brand

### ICU Skeleton Normalization

**Purpose**: Convert confusables to ASCII equivalents

```cpp
// Example: аpple.com → apple.com
uspoof_getSkeletonUTF8(checker, 0, "аpple.com", ..., skeleton, ...);
// skeleton = "apple.com"

// Compare skeleton to popular domains
if (skeleton == "apple.com" && original != "apple.com") {
    // Homograph attack detected!
}
```

## Browser Display Behavior

### Modern Browser Protections

1. **Punycode Display**:
   - Chrome/Firefox: Show punycode if mixed scripts detected
   - Safari: Shows punycode for suspicious patterns
   - Edge: Mixed-script domains shown as punycode

2. **Ladybird Behavior**:
   - PhishingURLAnalyzer flags homographs regardless of display
   - User receives security alert before navigation
   - PolicyGraph stores user decision (trust/block)

## Prevention Best Practices

### For Users

1. Always check the full URL, not just the beginning
2. Look for punycode (`xn--`) in address bar
3. Verify HTTPS certificate details (not just padlock)
4. Bookmark legitimate sites, don't type URLs manually
5. Enable Sentinel's phishing detection

### For Developers

1. Use ICU spoofchecker for confusable detection
2. Normalize URLs to skeleton form before comparison
3. Maintain database of popular domains for typosquatting checks
4. Alert users before navigating to suspicious domains
5. Allow user overrides but persist decisions

## Testing Resources

### Online Tools

- **Punycoder**: https://www.punycoder.com/
  - Convert between Unicode and Punycode
- **Unicode Character Inspector**: https://unicode-table.com/
  - View character details and confusables
- **ICU Spoofchecker Demo**: https://icu4c-demos.unicode.org/uspoof/
  - Test confusable detection

### Test Domains (DO NOT NAVIGATE)

These are for testing purposes only. Never enter credentials on test domains.

```
xn--pple-43d.com        # аpple.com (Cyrillic 'а')
xn--80ak6aa92e.com      # аррle.com (Cyrillic 'а', 'р' x2)
xn--ggle-0nda.com       # gοοgle.com (Greek 'ο' x2)
xn--aypal-r7e.com       # рaypal.com (Cyrillic 'р')
```

## References

1. **Unicode Technical Standard #39**: Unicode Security Mechanisms
   - https://www.unicode.org/reports/tr39/

2. **ICU User Guide**: Spoofchecker
   - https://unicode-org.github.io/icu/userguide/transforms/security.html

3. **ICANN IDN Guidelines**
   - https://www.icann.org/resources/pages/idn-guidelines-2011-09-02-en

4. **PhishTank**: Community phishing archive
   - https://phishtank.org/

5. **Sentinel PhishingURLAnalyzer Implementation**
   - `Services/Sentinel/PhishingURLAnalyzer.{h,cpp}`
   - `Services/RequestServer/URLSecurityAnalyzer.{h,cpp}`

## Updates

This database is maintained as part of Ladybird's Sentinel security system. To contribute new homograph patterns:

1. Add test case to `Tests/Playwright/fixtures/phishing/homograph-test.html`
2. Update `HOMOGRAPH_DATABASE` in `helpers/phishing-test-utils.ts`
3. Run test suite: `npx playwright test tests/security/phishing-detection.spec.ts`
4. Document in this file

Last Updated: 2025-11-01
