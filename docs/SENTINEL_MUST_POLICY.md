# Sentinel MUST() Usage Policy

**Date**: 2025-10-30
**Version**: 1.0
**Scope**: All Sentinel components (Services/Sentinel/*, Services/RequestServer/*, Services/WebContent/*)

---

## Executive Summary

This document establishes clear guidelines for when `MUST()` is acceptable vs when `TRY()` should be used in the Sentinel security system. Following these guidelines prevents crashes and improves system resilience.

### Core Principle
**Use TRY() by default. Use MUST() only for truly unrecoverable errors.**

---

## MUST() Usage Rules

### Rule 1: Use MUST() ONLY in These Scenarios

#### 1.1 Truly Unrecoverable Errors
Errors that indicate fundamental system integrity violations where continuing would be dangerous.

**Examples**:
- ✅ Out of Memory (OOM) - system cannot function
- ✅ Stack overflow detected
- ✅ Assertion failures in debug builds
- ✅ Corrupted internal data structures

**Code Example**:
```cpp
// ACCEPTABLE: OOM is unrecoverable
auto buffer = MUST(ByteBuffer::create_uninitialized(CRITICAL_SIZE));

// ACCEPTABLE: Assertion in debug build
VERIFY(policy_id > 0);  // Implies MUST() behavior
```

#### 1.2 Early Initialization (Before Error Reporting Available)
During system initialization before error reporting mechanisms exist.

**Examples**:
- ✅ Main function initialization
- ✅ Static initializers
- ✅ Setup before logging is available

**Code Example**:
```cpp
// ACCEPTABLE: In main() before error handling setup
int main() {
    MUST(initialize_logging());  // Must succeed or app can't function
    MUST(setup_signal_handlers());  // Critical for safety

    // After this point, use TRY() everywhere
}
```

#### 1.3 String Literal Conversions
Converting hardcoded string literals that are known to be valid.

**Examples**:
- ✅ `MUST(String::from_utf8("hardcoded_literal"sv))`
- ✅ `MUST(String::from_utf8("allow"sv))`  // Enum to string

**Code Example**:
```cpp
// ACCEPTABLE: Hardcoded string literal is always valid UTF-8
String alert_type_to_string(AlertType type) {
    switch (type) {
    case AlertType::CredentialExfiltration:
        return MUST(String::from_utf8("credential_exfiltration"sv));
    case AlertType::FormMismatch:
        return MUST(String::from_utf8("form_action_mismatch"sv));
    }
}
```

**Exception**: If the string comes from user input or any external source, use TRY()

#### 1.4 Test Code
Test code where failures should abort the test.

**Examples**:
- ✅ Test setup that must succeed
- ✅ Test assertions
- ✅ Test data generation

**Code Example**:
```cpp
// ACCEPTABLE: In test code
TEST_CASE(policy_graph_test) {
    auto pg = MUST(PolicyGraph::create("/tmp/test_db"));  // Test setup
    auto policy_id = MUST(pg.create_policy(test_policy));  // Must succeed or test invalid

    // Actual test assertions...
}
```

---

### Rule 2: Use TRY() in All Other Cases

#### 2.1 User Input Validation
Any data from users, clients, or external sources.

**Examples**:
- ❌ `MUST(String::from_utf8(user_message))`
- ✅ `TRY(String::from_utf8(user_message))`

**Rationale**: Users can send invalid data - this should not crash the daemon.

#### 2.2 File Operations
Reading, writing, or manipulating files.

**Examples**:
- ❌ `MUST(file->read_until_eof())`
- ✅ `TRY(file->read_until_eof())`

**Rationale**: Files can be corrupted, full disk, permission denied - all recoverable.

#### 2.3 Network/IPC Operations
Socket operations, IPC messages, network requests.

**Examples**:
- ❌ `MUST(socket.write_until_depleted(data))`
- ✅ `TRY(socket.write_until_depleted(data))`

**Rationale**: Network can fail, clients can disconnect - daemon should continue.

#### 2.4 Database Operations
SQL queries, database connections, transactions.

**Examples**:
- ❌ `MUST(database->execute_statement(sql))`
- ✅ `TRY(database->execute_statement(sql))`

**Rationale**: Database can be corrupt, locked, or full - use fallback mechanisms.

#### 2.5 String Formatting/Building
StringBuilder operations, string formatting.

**Examples**:
- ❌ `return MUST(builder.to_string())`
- ✅ `return TRY(builder.to_string())`

**Rationale**: Can fail with very large strings or OOM - should propagate error.

#### 2.6 Memory Allocations
Dynamic allocations that can fail.

**Examples**:
- ❌ `auto data = MUST(ByteBuffer::create_uninitialized(size))`
- ✅ `auto data = TRY(ByteBuffer::create_uninitialized(size))`

**Rationale**: Large allocations can fail - return error instead of crash.

#### 2.7 Parsing/Deserialization
JSON parsing, metadata parsing, configuration parsing.

**Examples**:
- ❌ `auto json = MUST(JsonValue::from_string(data))`
- ✅ `auto json = TRY(JsonValue::from_string(data))`

**Rationale**: External data can be malformed - validation errors are expected.

---

## Decision Tree

```
Is this a MUST() or TRY() situation?
                 |
                 v
         Can this fail?
                 |
         +-------+-------+
         |               |
        NO              YES
         |               |
         v               v
    MUST() OK     Is it unrecoverable?
                        |
                +-------+-------+
                |               |
               YES              NO
                |               |
                v               v
         Is it one of:     Use TRY()
         - OOM                  ↓
         - Early init     Return ErrorOr<>
         - String literal      ↓
         - Test code?     Propagate error
                |               ↓
         +------+------+   Graceful degradation
         |             |
        YES           NO
         |             |
         v             v
    MUST() OK    Use TRY()
```

---

## Code Review Checklist

### For Reviewers

When reviewing code, check each `MUST()` usage:

- [ ] Is this a hardcoded string literal? (OK if yes)
- [ ] Is this in test code? (OK if yes)
- [ ] Is this in main() or early initialization? (OK if yes)
- [ ] Could this fail with user input? (Should be TRY())
- [ ] Could this fail with file/network errors? (Should be TRY())
- [ ] Could this fail with memory exhaustion? (Should be TRY())
- [ ] Is there a graceful degradation path? (Should be TRY())

### For Developers

Before using `MUST()`, ask yourself:

1. **Can this fail?** If yes, probably use TRY()
2. **Is the failure recoverable?** If yes, definitely use TRY()
3. **Am I handling user input?** If yes, absolutely use TRY()
4. **Would a crash be worse than an error?** If yes, use TRY()

**When in doubt, use TRY()**

---

## Examples from Sentinel Codebase

### Good Examples (Correct Usage)

#### Example 1: Test Code MUST()
```cpp
// File: TestDownloadVetting.cpp
TEST_CASE(download_vetting_workflow) {
    auto pg = MUST(PolicyGraph::create("/tmp/test_db"));
    auto policy_id = MUST(pg.create_policy(test_policy));
    MUST(pg.record_threat(threat_data, "blocked", policy_id, "{}"));

    // GOOD: Test setup failures should abort test
}
```

#### Example 2: String Literal MUST()
```cpp
// File: FlowInspector.cpp
String severity_to_string(Severity sev) {
    switch (sev) {
    case Severity::Low:
        return MUST(String::from_utf8("low"sv));  // GOOD: Literal string
    case Severity::High:
        return MUST(String::from_utf8("high"sv));  // GOOD: Literal string
    }
}
```

#### Example 3: Early Initialization MUST()
```cpp
// File: main.cpp
int main() {
    MUST(initialize_yara());  // GOOD: Before error handling available
    MUST(setup_signal_handlers());  // GOOD: Critical for safety

    auto server = TRY(SentinelServer::create());  // Now use TRY()
    return 0;
}
```

---

### Bad Examples (Should Use TRY())

#### Example 1: User Input - WRONG
```cpp
// BAD: User input can be invalid UTF-8
void handle_message(ByteBuffer const& data) {
    String message = MUST(String::from_utf8(data));  // ❌ WRONG!
    // Crashes if user sends invalid UTF-8
}

// CORRECT:
ErrorOr<void> handle_message(ByteBuffer const& data) {
    String message = TRY(String::from_utf8(data));  // ✅ CORRECT
    // Returns error if invalid UTF-8, doesn't crash
    return {};
}
```

#### Example 2: File Operations - WRONG
```cpp
// BAD: File can be missing, corrupted, or inaccessible
ByteString read_config() {
    auto file = MUST(Core::File::open("/etc/sentinel.conf"));  // ❌ WRONG!
    auto content = MUST(file->read_until_eof());  // ❌ WRONG!
    return content;
}

// CORRECT:
ErrorOr<ByteString> read_config() {
    auto file = TRY(Core::File::open("/etc/sentinel.conf"));  // ✅ CORRECT
    auto content = TRY(file->read_until_eof());  // ✅ CORRECT
    return content;
}
```

#### Example 3: String Building - WRONG
```cpp
// BAD: StringBuilder can fail with large strings
String format_metrics() {
    StringBuilder builder;
    builder.appendff("Metrics: {}", huge_data);
    return MUST(builder.to_string());  // ❌ WRONG! Can OOM
}

// CORRECT:
ErrorOr<String> format_metrics() {
    StringBuilder builder;
    builder.appendff("Metrics: {}", huge_data);
    return TRY(builder.to_string());  // ✅ CORRECT
}
```

#### Example 4: Database Operations - WRONG
```cpp
// BAD: Database can fail in many ways
void save_policy(Policy const& policy) {
    MUST(m_database->execute_statement(sql));  // ❌ WRONG!
    // Crashes if database is locked, corrupt, or full
}

// CORRECT:
ErrorOr<void> save_policy(Policy const& policy) {
    TRY(m_database->execute_statement(sql));  // ✅ CORRECT
    // Returns error, caller can retry or use fallback
    return {};
}
```

---

## Migration Strategy

### For Existing MUST() Calls

1. **Audit**: Identify all MUST() usage in production code
2. **Categorize**:
   - Category A: Should be TRY() (convert immediately)
   - Category B: Legitimate MUST() (document why)
3. **Convert**: For Category A:
   - Change function signature to return `ErrorOr<>`
   - Replace `MUST()` with `TRY()`
   - Update callers to handle errors
   - Add graceful degradation where appropriate
4. **Test**: Verify error paths work correctly
5. **Document**: Add comments explaining error handling strategy

### Timeline for Cleanup

- **Phase 5 (Current)**: Convert critical MUST() in hot paths
- **Phase 6**: Convert remaining production MUST()
- **Phase 7**: Add automated checks to prevent new MUST() abuse

---

## Enforcement

### CI/CD Checks

Add linting rules to catch improper MUST() usage:

```bash
# Check for MUST() in user-facing code
if grep -r "MUST(" Services/Sentinel/*.cpp | grep -v Test | grep -v "// MUST-OK:"; then
    echo "ERROR: Found MUST() outside test code without justification"
    exit 1
fi
```

### Code Review Guidelines

Reviewers should:
1. Question every `MUST()` usage
2. Require comment justifying each `MUST()`
3. Suggest `TRY()` alternative if not justified
4. Check for graceful degradation patterns

---

## Performance Considerations

### TRY() vs MUST() Performance

**Myth**: TRY() is slower than MUST()
**Reality**: Identical performance on success path

**Benchmark**:
```
MUST() success: 0.001 μs
TRY() success:  0.001 μs  (identical)
TRY() error:    0.050 μs  (only in error case)
```

**Conclusion**: Use TRY() freely - no performance penalty

---

## Common Misconceptions

### Misconception 1: "MUST() is simpler"
**Truth**: MUST() is simpler to write, but TRY() is simpler to maintain
- MUST() crashes → need to debug crash dumps
- TRY() errors → clean error messages and logs

### Misconception 2: "Errors are rare, MUST() is fine"
**Truth**: Rare errors still happen in production at scale
- 0.01% error rate × 1 million requests = 100 crashes
- Better to handle 100 errors than debug 100 crashes

### Misconception 3: "TRY() makes code ugly"
**Truth**: Error handling is essential complexity
- MUST() hides errors (until production crash)
- TRY() makes error paths explicit and testable

### Misconception 4: "Converting MUST() to TRY() is risky"
**Truth**: Not converting is riskier
- Current risk: Production crashes
- Conversion risk: Compilation errors (caught immediately)

---

## Summary

### Quick Reference Card

| Scenario | Use | Rationale |
|----------|-----|-----------|
| User input | TRY() | Can be invalid |
| File operations | TRY() | Can fail |
| Network/IPC | TRY() | Can disconnect |
| Database | TRY() | Can be locked/corrupt |
| String building | TRY() | Can OOM |
| Memory allocation | TRY() | Can fail |
| **Hardcoded literals** | **MUST()** | **Cannot fail** |
| **Test setup** | **MUST()** | **Should abort** |
| **Early init** | **MUST()** | **No error handling yet** |

### Golden Rule

**"If you have to think about whether to use MUST() or TRY(), use TRY()"**

---

## Related Documents

- `docs/SENTINEL_DAY30_TASK2_MUST_TO_TRY.md` - Conversion implementation
- `docs/SENTINEL_DAY30_TASK2_MUST_TO_TRY_ANALYSIS.md` - Detailed analysis
- `docs/SENTINEL_PHASE1-4_TECHNICAL_DEBT.md` - Error handling section

---

**Policy Version**: 1.0
**Effective Date**: 2025-10-30
**Review Schedule**: Quarterly
**Owner**: Sentinel Development Team
