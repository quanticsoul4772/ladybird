# Credential Protection User Guide

**Sentinel Phase 6 - Milestone 0.2**

This guide explains how Ladybird protects your passwords and sensitive information from credential exfiltration attacks.

---

## Table of Contents

1. [Overview](#overview)
2. [What is Credential Exfiltration?](#what-is-credential-exfiltration)
3. [How Protection Works](#how-protection-works)
4. [Understanding Security Alerts](#understanding-security-alerts)
5. [Managing Trusted Forms](#managing-trusted-forms)
6. [Autofill Protection](#autofill-protection)
7. [Best Practices](#best-practices)
8. [Troubleshooting](#troubleshooting)
9. [Privacy and Data Storage](#privacy-and-data-storage)

---

## Overview

Ladybird includes built-in credential protection that monitors form submissions in real-time to detect and prevent credential theft attempts. This protection works automatically without requiring any configuration, but gives you full control when suspicious activity is detected.

### Key Features

- **Real-time Monitoring**: Every form submission with password fields is analyzed
- **Cross-Origin Detection**: Alerts when credentials are sent to unexpected domains
- **Trusted Relationships**: Remember legitimate forms to avoid repeated alerts
- **Autofill Protection**: Blocks password autofill on suspicious forms
- **User Education**: Learn about threats and make informed decisions

---

## What is Credential Exfiltration?

Credential exfiltration is an attack where malicious websites attempt to steal your login credentials (usernames and passwords). Common techniques include:

### Attack Vectors

1. **Cross-Origin Form Submission**
   - A form on `trusted-site.com` submits to `attacker.com`
   - Your credentials are sent to the attacker's server
   - Example: Hidden iframe with malicious form

2. **Insecure Connections**
   - Form submits credentials over HTTP instead of HTTPS
   - Credentials can be intercepted in transit
   - Man-in-the-middle attacks

3. **Third-Party Embedded Forms**
   - Legitimate site embeds form from different origin
   - May be intentional (OpenID) or malicious (phishing)
   - Difficult for users to distinguish

4. **Form Action Mismatch**
   - Form appears to be for one site but submits elsewhere
   - Spoofed login pages
   - Phishing attempts

---

## How Protection Works

Ladybird's FormMonitor component runs in the WebContent process and intercepts form submissions before they occur.

### Detection Process

```
User submits form
       ↓
FormMonitor intercepts submission
       ↓
Checks for password fields → No password? Allow submission
       ↓
Checks form action origin
       ↓
Same origin? → Allow submission
       ↓
Cross-origin detected
       ↓
Check trusted relationships → Trusted? Allow submission
       ↓
Not trusted → Trigger security alert
       ↓
User decides: Block | Trust | Learn More
```

### What Gets Monitored

FormMonitor analyzes:
- **Form submission events** (when user clicks submit)
- **Password field detection** (input[type="password"])
- **Form action URLs** (where data is being sent)
- **Origin comparison** (form page vs. action destination)
- **Connection security** (HTTP vs. HTTPS)
- **Form anomalies** (suspicious patterns indicating phishing or data harvesting)

### Advanced Anomaly Detection (Milestone 0.3 Phase 6)

Beyond basic cross-origin detection, Ladybird uses advanced heuristics to identify sophisticated phishing and data harvesting attempts:

#### 1. Hidden Field Ratio Analysis (30% weight)
- **What it detects**: Forms with excessive hidden fields often indicate phishing
- **Normal forms**: 10-20% hidden fields (CSRF tokens, session IDs)
- **Suspicious forms**: >50% hidden fields (data collection, credential harvesting)
- **Example**: A login form with 10 fields where 6 are hidden

#### 2. Field Count Analysis (20% weight)
- **What it detects**: Unusually high number of fields suggests data harvesting
- **Normal forms**: 2-15 fields
- **Suspicious forms**: >25 fields (profile scraping, mass data collection)
- **Example**: A "simple login" form with 30+ input fields

#### 3. Domain Reputation Check (30% weight)
- **Free TLDs**: `.tk`, `.ml`, `.ga`, `.cf`, `.gq` commonly used in phishing
- **Suspicious keywords**: `phishing`, `fake-`, `scam`, `data-collect`, `harvester`
- **IP addresses**: Forms submitting to raw IP addresses instead of domains
- **Long domains**: >40 characters may indicate typosquatting
- **Example**: `secure-bank-login-verification.tk/auth`

#### 4. Submission Frequency Analysis (20% weight)
- **What it detects**: Bot-like rapid form submissions
- **Normal behavior**: 1-2 submissions per minute
- **Suspicious behavior**: >5 submissions in 5 seconds
- **Automated attacks**: Consistent timing patterns (variance <1s)

#### Anomaly Scoring

Each form submission receives an anomaly score (0.0-1.0):
- **0.0-0.2**: Normal, no additional warnings
- **0.2-0.5**: Mildly suspicious, noted in alert details
- **0.5-0.8**: Highly suspicious, prominent warning
- **0.8-1.0**: Extremely suspicious, strong block recommendation

When multiple anomalies are detected, the scores combine to provide an overall risk assessment. High-scoring forms trigger enhanced security warnings with detailed anomaly indicators.

---

## Understanding Security Alerts

When Ladybird detects a suspicious form submission, you'll see a **Credential Exfiltration Warning** dialog.

### Alert Dialog Components

#### 1. Header
```
⚠️ Credential Exfiltration Warning
```
Indicates that Sentinel detected suspicious credential submission.

#### 2. Alert Details

- **Form Origin**: The website where the form appears
- **Submitting To**: Where your credentials would be sent
- **Alert Type**: Specific threat category
- **Severity**: Critical, High, Medium, or Low
- **Description**: Human-readable explanation
- **Security Indicators**: Protocol, cross-origin status, password field

#### 3. Action Buttons

**🚫 Block**
- Prevents the form submission immediately
- Recommended for unknown or suspicious forms
- No data is sent to the action URL

**✓ Trust**
- Allows this specific form to submit credentials
- Creates a trusted relationship for this form-action pair
- Future submissions from this form won't trigger alerts
- Use only if you recognize and trust both domains

**ℹ️ Learn More**
- Opens about:security with education modal
- Learn about credential exfiltration threats
- Form submission is blocked while you learn
- Return to the form after reading

### Alert Types

#### credential_exfiltration
**Severity**: Critical or High

The form is sending credentials to a completely different domain than the page you're on.

**Example**: You're on `mybank.com` but the form submits to `evil-site.com`.

**Recommendation**: Block unless you specifically trust the destination (e.g., OpenID provider).

#### insecure_credential_post
**Severity**: High

The form is submitting credentials over an insecure HTTP connection.

**Example**: Form action is `http://site.com/login` instead of `https://site.com/login`.

**Recommendation**: Block. Legitimate sites should always use HTTPS for login forms.

#### third_party_form_post
**Severity**: Medium to High

The form is hosted on one domain but submits to another, which could be legitimate (OAuth/OpenID) or malicious.

**Example**: Login form on `app.com` submits to `accounts.google.com` (legitimate OAuth).

**Recommendation**: Trust if you recognize the authentication provider, block otherwise.

#### form_action_mismatch
**Severity**: Medium

The form's action URL doesn't match expected patterns for the site.

**Example**: Form on `amazon.com` submits to `amazon-login.com`.

**Recommendation**: Verify the action URL is correct before trusting.

---

## Managing Trusted Forms

### Viewing Trusted Forms

1. Navigate to `about:security`
2. Scroll to the **Credential Protection** card
3. Find the **Trusted Form Relationships** table

### Trust Information

For each trusted form, you'll see:

- **Form Origin**: Website where the form appears
- **Action Origin**: Where credentials are sent
- **Learned On**: Date you trusted this relationship
- **Submission Count**: How many times you've submitted (informational, tracked in WebContent)

### Revoking Trust

If you no longer want to trust a form:

1. Click the **Revoke Trust** button next to the relationship
2. Confirm the revocation
3. Future submissions from this form will trigger alerts again

### When to Revoke Trust

- You no longer use the service
- You suspect the trusted form was compromised
- You made a mistake trusting the form
- You want to be alerted again for verification

---

## Autofill Protection

Ladybird blocks password autofill on cross-origin forms to prevent silent credential theft.

### How Autofill Blocking Works

```
User focuses password field
       ↓
Browser attempts autofill
       ↓
FormMonitor.can_autofill() called
       ↓
Same origin? → Allow autofill
       ↓
Trusted form? → Allow autofill
       ↓
One-time override granted? → Allow autofill once
       ↓
Block autofill + Show banner notification
```

### Autofill Blocked Banner

When autofill is blocked, you'll see an **orange banner** at the top of the page:

```
⚠️ Autofill blocked for security reasons
This form is attempting to send credentials to a different origin.
[Allow Once] [Dismiss]
```

### Allowing Autofill Once

If you trust the form and want to use autofill:

1. Click **Allow Once** in the banner
2. The browser grants a one-time autofill override
3. Manually trigger autofill (usually by focusing the field again)
4. Autofill proceeds normally **for this one instance only**
5. Next time, autofill will be blocked again unless you trust the form

### Why Autofill is Blocked

Autofill blocking protects against:

- **Hidden form fields** that autofill without your knowledge
- **Background form submissions** that steal credentials silently
- **Embedded phishing forms** that harvest credentials
- **JavaScript-triggered autofill** on malicious forms

### Legitimate Use Cases

Some legitimate scenarios require cross-origin autofill:

1. **OpenID/OAuth Login**
   - You're redirected to an identity provider (e.g., Google, Facebook)
   - The login form is cross-origin by design
   - Trust the provider to enable autofill

2. **Single Sign-On (SSO)**
   - Corporate SSO providers often use separate domains
   - Trust your organization's SSO domain

3. **Payment Providers**
   - Secure payment forms may be hosted on separate domains
   - Verify the domain before trusting

---

## Best Practices

### For Users

#### ✅ Do

1. **Verify URLs** before entering credentials
   - Check the domain in the address bar
   - Look for HTTPS with a valid certificate
   - Beware of similar-looking domains (e.g., `g00gle.com` vs `google.com`)

2. **Review alerts carefully**
   - Read the Form Origin and Action Origin
   - If they don't match what you expect, block the submission
   - Use "Learn More" if you're unsure

3. **Trust legitimate services**
   - Major authentication providers (Google, Microsoft, etc.)
   - Your company's SSO system
   - Known payment processors

4. **Review trusted forms periodically**
   - Visit `about:security` monthly
   - Revoke unused or unrecognized trusts
   - Stay aware of what forms have access

5. **Keep Ladybird updated**
   - Security improvements are added regularly
   - Updates may include new detection methods

#### ❌ Don't

1. **Don't trust unfamiliar domains**
   - Unknown domains requesting credentials are suspicious
   - When in doubt, block and navigate to the official site manually

2. **Don't trust insecure connections**
   - Never trust forms submitting over HTTP
   - Legitimate services always use HTTPS for credentials

3. **Don't ignore alerts**
   - Alerts indicate real security concerns
   - Take time to understand what's happening

4. **Don't reuse passwords**
   - If a credential is stolen, it won't compromise other accounts
   - Use a password manager

5. **Don't disable protection**
   - The credential protection system cannot be disabled
   - This is intentional for your security

### For Web Developers

If you're developing a website and your users see credential exfiltration alerts:

#### Same-Origin Forms

Ensure your login forms submit to the same origin:

```html
<!-- Good: Same origin -->
<form action="/login" method="POST">

<!-- Good: Explicit same origin -->
<form action="https://yoursite.com/login" method="POST">

<!-- Bad: Cross-origin -->
<form action="https://different-site.com/login" method="POST">
```

#### OpenID/OAuth Flows

If you use third-party authentication:

1. **Redirect-based flows** won't trigger alerts (user navigates to provider)
2. **Embedded iframe flows** may trigger alerts if the form is cross-origin
3. Consider using redirect-based flows for better security

#### HTTPS for Credentials

Always use HTTPS for forms handling credentials:

```html
<!-- Good -->
<form action="https://yoursite.com/login" method="POST">

<!-- Bad - triggers insecure_credential_post alert -->
<form action="http://yoursite.com/login" method="POST">
```

#### Test Your Forms

Test login flows in Ladybird to see if alerts are triggered. If legitimate use causes alerts, consider:

1. Using same-origin form submissions
2. Redirecting to authentication providers instead of embedding
3. Documenting for users that your service uses cross-origin authentication

---

## Troubleshooting

### "I trust this site but keep getting alerts"

**Solution**: Click "Trust" in the alert dialog. Ladybird will remember your decision and won't alert you again for this specific form-action pair.

### "I trusted a form by mistake"

**Solution**:
1. Navigate to `about:security`
2. Find the form in the **Trusted Form Relationships** table
3. Click **Revoke Trust**
4. Confirm the revocation

### "Autofill doesn't work on my banking site"

**Possible reasons**:

1. **Cross-origin form submission** - Your bank may be using a third-party authentication service
   - **Solution**: Click "Allow Once" when the banner appears, or trust the form after verifying the action domain

2. **Bank recently changed domains** - Previously trusted relationship no longer applies
   - **Solution**: Check about:security and revoke old trusts, then trust the new relationship

3. **Autofill override not granted** - You need to explicitly allow autofill
   - **Solution**: Look for the orange banner and click "Allow Once"

### "I want to see credential education again"

**Solution**: Navigate to `about:security`, scroll to **Security Tips**, and click **Learn How Protection Works**.

### "The alert doesn't explain why it's suspicious"

**Solution**:
1. Click "Learn More" in the alert dialog
2. Read the education modal for detailed threat explanations
3. Check the Security Tips section in about:security

### "Can I disable credential protection?"

**Answer**: No. Credential protection is a core security feature and cannot be disabled. You have full control to trust specific forms, but the monitoring system itself always runs.

---

## Privacy and Data Storage

### What Data is Collected

Ladybird's credential protection system collects minimal data:

1. **Trusted Relationships** (in memory only)
   - Form origin (e.g., `https://example.com`)
   - Action origin (e.g., `https://auth.provider.com`)
   - No password data
   - No form content
   - No personally identifiable information

2. **Autofill Overrides** (in memory only)
   - Form-action origin pairs
   - Single-use flags
   - Cleared when you navigate away

3. **Education Preference** (on disk)
   - File: `~/.local/share/Ladybird/credential_education_shown`
   - Contains: "1" if you checked "Don't show this again"
   - Purpose: Prevent showing education modal repeatedly

### What Data is NOT Collected

- ❌ Passwords or credentials
- ❌ Form field values
- ❌ Browsing history
- ❌ Personal information
- ❌ Analytics or telemetry

### Data Persistence

| Data Type | Storage | Persistence |
|-----------|---------|-------------|
| Trusted relationships | Memory (WebContent process) | Session only |
| Autofill overrides | Memory (WebContent process) | Current page only |
| Education preference | Disk (`~/.local/share/Ladybird/`) | Permanent until deleted |

### Future Enhancements

**Note**: As of Milestone 0.2, trusted relationships are **not persisted** across browser restarts. Future versions may add optional persistence to PolicyGraph database.

### Data Security

- All credential protection logic runs **locally** in your browser
- No data is sent to external servers
- No network requests are made by the credential protection system
- All monitoring happens **before** form submission, protecting your credentials

---

## FAQ

### Q: Will this slow down my browsing?

**A**: No. FormMonitor only activates when you submit forms with password fields. The overhead is negligible (typically <1ms per submission check).

### Q: Does this work with password managers?

**A**: Yes. Password managers work normally. If your password manager auto-submits forms, Ladybird will still check them before submission proceeds.

### Q: What about mobile browsers?

**A**: This feature is implemented in LibWeb and works across all Ladybird platforms. However, the UI dialogs may differ on mobile.

### Q: Can websites detect this protection?

**A**: Websites cannot reliably detect that FormMonitor is running. Blocked submissions appear as normal "form submission cancelled" events.

### Q: What if a legitimate site triggers false positives?

**A**: Some legitimate sites use cross-origin authentication (OpenID, OAuth, SSO). These are intentionally flagged for your awareness. Trust them after verifying the action domain is legitimate.

### Q: How does this compare to browser password managers?

**A**: This is complementary security:
- **Password managers**: Store and autofill credentials
- **Credential protection**: Prevents credentials from being sent to wrong destinations

Use both for maximum security.

### Q: What happens to forms submitted via JavaScript?

**A**: FormMonitor intercepts programmatic form submissions as well. Even if JavaScript calls `form.submit()`, the submission is checked.

### Q: Can I export my trusted forms list?

**A**: Not yet. Currently, trusted relationships are stored in memory only. Future versions may add export/import functionality.

### Q: Does this protect against keyloggers?

**A**: No. Credential protection guards against **network-based credential theft** (exfiltration). It does not protect against:
- Keyloggers on your computer
- Screen capture malware
- Physical security breaches

Use antivirus software and keep your system secure.

### Q: What about two-factor authentication (2FA)?

**A**: Credential protection monitors password submissions. 2FA codes are typically entered after initial login, often on the same origin, so they usually don't trigger alerts. However, if a site sends 2FA codes cross-origin, you'll be alerted.

---

## Getting Help

### Resources

- **Education Modal**: about:security → "Learn How Protection Works" button
- **Security Center**: about:security for managing trusted forms
- **Sentinel Documentation**: See `docs/SENTINEL_PHASE6_*.md` in the Ladybird repository
- **Issue Tracker**: Report bugs at https://github.com/LadybirdBrowser/ladybird/issues

### Reporting Issues

If you encounter a problem:

1. **Check about:security** to see current trust relationships
2. **Try revoking and re-trusting** the form
3. **Clear browser data** and try again
4. **Report the issue** with:
   - Form origin URL
   - Action origin URL
   - Alert type and severity
   - Expected vs. actual behavior
   - Do NOT include passwords or sensitive data

### Contributing

Credential protection is part of the Sentinel security system. Contributions are welcome:

- Code: `Services/WebContent/FormMonitor.{h,cpp}`
- UI: `UI/Qt/SecurityAlertDialog.{h,cpp}`, `UI/Qt/Tab.cpp`
- Tests: `Tests/LibWeb/Text/input/credential-protection-*.html`
- Documentation: This guide

---

## Version History

### Milestone 0.3 - Enhanced Credential Protection (2025-10-31)

- ✅ Persistent trusted/blocked form relationships (Phase 1-3)
- ✅ Credential alert history database (Phase 1-3)
- ✅ Import/export trusted relationships (Phase 4)
- ✅ Policy template system (Phase 5)
- ✅ Advanced form anomaly detection (Phase 6)
  - Hidden field ratio analysis
  - Field count anomaly detection
  - Domain reputation checking
  - Submission frequency monitoring
  - Multi-factor anomaly scoring (0.0-1.0)
  - Anomaly indicators in alert metadata

### Milestone 0.2 - Core Credential Protection (2025-10-29)

- ✅ Real-time form submission monitoring
- ✅ Cross-origin detection
- ✅ Security alert dialogs
- ✅ Trusted form relationships
- ✅ Autofill protection
- ✅ One-time autofill override
- ✅ about:security integration
- ✅ User education modal
- ✅ Security tips section
- ✅ End-to-end tests
- ✅ User guide documentation

### Future Roadmap

- 🔮 Machine learning-based anomaly detection
- 🔮 Federated threat intelligence sharing
- 🔮 Browser fingerprinting detection
- 🔮 Advanced phishing URL analysis
- 🔮 Browser extension API for third-party security tools

---

## Legal

Credential protection is provided as-is without warranty. While Ladybird makes a best effort to detect credential exfiltration attempts, no security system is perfect. Users should:

- Verify websites before entering credentials
- Use unique passwords for each service
- Enable two-factor authentication where available
- Keep their browser and system software updated

**This feature is experimental and subject to change.**

---

*Last updated: Milestone 0.3 Phase 6 (2025-10-31)*
*Document version: 1.0*
*Ladybird Browser - Protecting your credentials*
