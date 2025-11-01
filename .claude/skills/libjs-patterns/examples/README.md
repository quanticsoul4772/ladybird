# LibJS Pattern Examples

This directory contains complete, production-quality examples of common LibJS patterns.

## Files

### native-function-example.cpp
Complete example of implementing a native function (`String.prototype.repeat`):
- Function registration in prototype
- Proper error handling with ThrowCompletionOr
- Type conversions and validation
- ECMAScript spec compliance
- Comments explaining key patterns

**Use this when**: Implementing built-in methods for JavaScript objects

### object-implementation-example.cpp
Complete object type implementation (Point with x, y coordinates):
- Custom object class with GC integration
- Constructor function
- Prototype with instance methods
- Property getters/setters
- Static methods
- Full three-tier architecture

**Use this when**: Creating new JavaScript object types

### gc-integration-example.cpp
Comprehensive garbage collection patterns:
- Using GC::Ref and GC::Ptr correctly
- Implementing visit_edges for marking
- Allocation patterns
- Collections of GC objects
- Common pitfalls and anti-patterns

**Use this when**: Working with GC-managed objects or debugging GC issues

### bytecode-operation-example.cpp
Bytecode system patterns:
- Defining new bytecode operations
- Implementing execution logic
- Code generation from AST
- Register allocation
- Control flow (jumps, conditionals)

**Use this when**: Adding new bytecode operations or understanding the bytecode system

## How to Use These Examples

1. **Read the SKILL.md first** for conceptual overview
2. **Find the relevant example** for your use case
3. **Copy and adapt** the pattern to your needs
4. **Refer to references/** for API details

## Related Documentation

- **SKILL.md**: Main skill documentation with all patterns
- **references/libjs-architecture.md**: VM and runtime architecture
- **references/ecmascript-compliance.md**: Spec compliance guidelines
- **references/common-apis.md**: Frequently used APIs
- **references/performance-tips.md**: Optimization patterns
