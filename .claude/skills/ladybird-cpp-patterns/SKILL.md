---
name: ladybird-cpp-patterns
description: Modern C++ patterns and idioms specific to Ladybird browser development, including RefPtr usage, ErrorOr handling, and SerenityOS style conventions
use-when: Writing or reviewing C++ code in Ladybird codebase
allowed-tools:
  - Read
  - Write
  - Grep
  - Glob
---

# Ladybird C++ Patterns

## Core Patterns

### 1. Smart Pointers (RefPtr/NonnullRefPtr)

**Always use RefPtr for heap-allocated objects:**
```cpp
// ✅ CORRECT - Reference counted
class MyClass : public RefCounted<MyClass> {
public:
    static ErrorOr<NonnullRefPtr<MyClass>> create()
    {
        return adopt_ref(*new MyClass());
    }
private:
    MyClass() = default;
};

// Usage
auto obj = TRY(MyClass::create());

// ❌ WRONG - Raw pointer
MyClass* obj = new MyClass();
```

### 2. ErrorOr<T> for Error Handling

**All functions that can fail return ErrorOr<T>:**
```cpp
// ✅ CORRECT - Explicit error handling
ErrorOr<void> process_data(ByteBuffer const& data)
{
    if (data.size() < MIN_SIZE)
        return Error::from_string_literal("Data too small");

    TRY(validate_data(data));
    TRY(parse_data(data));
    return {};
}

// ❌ WRONG - No error handling
void process_data(ByteBuffer const& data)
{
    // Silent failure or crash
}
```

### 3. RAII and Resource Management
```cpp
// ✅ CORRECT - RAII pattern
class FileReader {
public:
    static ErrorOr<FileReader> open(StringView path)
    {
        auto fd = TRY(Core::File::open(path, Core::File::OpenMode::Read));
        return FileReader(move(fd));
    }

    ~FileReader() { /* fd closed automatically */ }

private:
    explicit FileReader(NonnullOwnPtr<Core::File> fd)
        : m_file(move(fd)) {}

    NonnullOwnPtr<Core::File> m_file;
};
```

### 4. Move Semantics
```cpp
// ✅ CORRECT - Move expensive objects
ErrorOr<void> send_message(IPC::Message&& message)
{
    return m_connection->send(move(message));
}

// ❌ WRONG - Unnecessary copy
ErrorOr<void> send_message(IPC::Message message)
{
    return m_connection->send(message); // Copy!
}
```

## IPC-Specific Patterns

### Message Validation
```cpp
// ✅ CORRECT - Always validate before processing
IPC::ResponseOr<void> handle_message(Messages::WebContent::LoadURL const& message)
{
    // 1. Validate size limits
    if (message.url().length() > MAX_URL_LENGTH)
        return IPC::Error::InvalidMessage;

    // 2. Validate format
    auto url = URL::create_with_url_or_path(message.url());
    if (!url.is_valid())
        return IPC::Error::InvalidMessage;

    // 3. Check privileges
    if (!can_load_url(url))
        return IPC::Error::PermissionDenied;

    // 4. Process
    TRY(load_url(url));
    return {};
}
```

## Naming Conventions
```cpp
// Member variables: m_ prefix
class MyClass {
    int m_count { 0 };
    String m_name;
};

// Static members: s_ prefix
static int s_global_count { 0 };

// Functions: snake_case
void process_input();

// Classes/Structs: PascalCase
class WebContentClient;

// Constants: SCREAMING_SNAKE_CASE
constexpr size_t MAX_BUFFER_SIZE = 4096;
```

## Common Pitfalls

### ❌ Don't use exceptions
```cpp
// Exceptions are disabled in Ladybird
// Use ErrorOr instead
```

### ❌ Don't use raw new/delete
```cpp
// Use adopt_ref, adopt_own, or make()
auto obj = make<MyClass>();
```

### ❌ Don't use std::string
```cpp
// Use AK::String or AK::StringView
String name = "example"sv;
```

## Checklist

When writing Ladybird C++:

- [ ] Use RefPtr/NonnullRefPtr for heap objects
- [ ] Return ErrorOr<T> for fallible operations
- [ ] Use TRY() macro for error propagation
- [ ] Apply move semantics for expensive objects
- [ ] Follow naming conventions (m_, s_ prefixes)
- [ ] No raw pointers, new/delete
- [ ] No exceptions
- [ ] Use AK types (String, Vector, HashMap)
- [ ] RAII for all resources
- [ ] Const correctness

## Related Skills

### Foundational for Other Skills
This skill provides C++ patterns used across all other skills. Reference when:
- **[ipc-security](../ipc-security/SKILL.md)**: Implementing IPC handlers with ErrorOr<T> and SafeMath patterns
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Managing process lifecycle with smart pointers
- **[libweb-patterns](../libweb-patterns/SKILL.md)**: Implementing Web APIs with spec-compliant naming
- **[libjs-patterns](../libjs-patterns/SKILL.md)**: Implementing JavaScript built-ins with GC integration
- **[tor-integration](../tor-integration/SKILL.md)**: Implementing privacy features with RefPtr and ErrorOr
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Writing fuzzer harnesses with proper error handling

### Code Quality Skills
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug issues found when violating these patterns. Use sanitizers to catch smart pointer misuse and ErrorOr violations.
- **[documentation-generation](../documentation-generation/SKILL.md)**: Document C++ APIs using Doxygen. Follow documentation patterns for ErrorOr, smart pointers, and class hierarchies.

### Build and Testing
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Build projects using these patterns. Understand C++23 requirements and compiler flags.
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Test code using these patterns. Write spec-compliant implementations with ErrorOr error handling.
- **[ladybird-git-workflow](../ladybird-git-workflow/SKILL.md)**: Commit code following these patterns. Use spec-matching names in commit messages.
