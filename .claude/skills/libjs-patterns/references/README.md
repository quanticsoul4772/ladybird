# LibJS Reference Documentation

In-depth reference material for LibJS development.

## Files

### libjs-architecture.md
Comprehensive overview of LibJS architecture:
- High-level system architecture diagram
- VM, Heap, Runtime components
- Bytecode interpreter design
- Execution contexts and realms
- Environment (scope) management
- Value representation (NanBoxing)
- GC integration
- Performance characteristics

**Read this**: To understand how LibJS works internally

### ecmascript-compliance.md
Guidelines for implementing ECMAScript specifications:
- Naming conventions (match spec exactly!)
- Step-by-step implementation patterns
- Abstract operations mapping
- Completion records and error handling
- Type conversions
- Property descriptors
- Common spec patterns
- Test262 compliance

**Read this**: Before implementing any JavaScript feature

### common-apis.md
Quick reference for frequently used APIs:
- VM and execution context access
- Value operations (type checking, conversion, creation)
- Object operations (property access, definition)
- String operations
- Array operations
- Function calls
- Error handling
- Realm and intrinsics
- Abstract operations
- GC operations

**Read this**: When you need to look up specific API usage

### performance-tips.md
Optimization patterns and anti-patterns:
- Fast integer arithmetic
- Inline caches
- String interning and StringBuilder
- Array optimization (packed vs sparse)
- Minimizing GC pressure
- Bytecode builtins
- Common anti-patterns
- Profiling techniques

**Read this**: When optimizing performance-critical code

## Usage Flow

1. **Starting out?** → Read libjs-architecture.md
2. **Implementing a feature?** → Read ecmascript-compliance.md
3. **Need an API?** → Look up common-apis.md
4. **Optimizing?** → Read performance-tips.md
5. **Debugging?** → Check architecture and common-apis

## Additional Resources

- **examples/**: Practical code examples
- **SKILL.md**: Complete patterns guide
- **Libraries/LibJS/**: Source code
- **Tests/LibJS/**: Test suite examples
- ECMAScript Spec: https://tc39.es/ecma262/
