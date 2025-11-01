# LibJS Architecture Overview

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      JavaScript Source                       │
└─────────────────────────────────┬───────────────────────────┘
                                  │
                                  ▼
                          ┌───────────────┐
                          │     Lexer     │ (Tokenization)
                          └───────┬───────┘
                                  │
                                  ▼
                          ┌───────────────┐
                          │     Parser    │ (Syntax analysis)
                          └───────┬───────┘
                                  │
                                  ▼
                          ┌───────────────┐
                          │      AST      │ (Abstract Syntax Tree)
                          └───────┬───────┘
                                  │
                                  ▼
                    ┌─────────────────────────┐
                    │   Bytecode Generator    │
                    └──────────┬──────────────┘
                               │
                               ▼
                    ┌─────────────────────────┐
                    │      Bytecode           │
                    └──────────┬──────────────┘
                               │
                               ▼
        ┌──────────────────────────────────────────────┐
        │          Bytecode Interpreter                 │
        │                                              │
        │  ┌──────────────────────────────────────┐  │
        │  │         Execution Loop                │  │
        │  │  - Fetch instruction                  │  │
        │  │  - Decode operands                    │  │
        │  │  - Execute operation                  │  │
        │  │  - Update registers                   │  │
        │  └──────────────────────────────────────┘  │
        └───────────────┬──────────────────────────────┘
                        │
                        ▼
        ┌──────────────────────────────────────────────┐
        │              Runtime                          │
        │                                              │
        │  ┌──────────────┐  ┌──────────────┐         │
        │  │     VM       │  │     Heap     │         │
        │  │  - Contexts  │  │  - Objects   │         │
        │  │  - Stack     │  │  - GC        │         │
        │  └──────────────┘  └──────────────┘         │
        │                                              │
        │  ┌──────────────────────────────────────┐  │
        │  │         Built-in Objects              │  │
        │  │  Object, Array, String, Function...   │  │
        │  └──────────────────────────────────────┘  │
        └──────────────────────────────────────────────┘
```

## Core Components

### 1. VM (Virtual Machine)

**Location**: `Libraries/LibJS/Runtime/VM.h`

The VM is the central execution environment that manages:

- **Execution Context Stack**: Tracks function call chain
- **Heap**: Garbage-collected memory for objects
- **Well-Known Symbols**: Symbol.iterator, Symbol.toStringTag, etc.
- **String Cache**: Deduplicates string allocations
- **Error Handling**: Exception propagation

```cpp
class VM : public RefCounted<VM> {
    // Execution contexts
    Vector<ExecutionContext*> m_execution_context_stack;

    // Garbage-collected heap
    GC::Heap m_heap;

    // Bytecode interpreter
    OwnPtr<Bytecode::Interpreter> m_bytecode_interpreter;

    // Well-known symbols
    struct WellKnownSymbols {
        GC::Ptr<Symbol> iterator;
        GC::Ptr<Symbol> to_string_tag;
        // ... more symbols
    } m_well_known_symbols;

    // String interning
    HashMap<String, GC::Ptr<PrimitiveString>> m_string_cache;
};
```

**Key Methods**:
- `running_execution_context()`: Get current execution context
- `push_execution_context()`: Enter function call
- `pop_execution_context()`: Exit function call
- `throw_completion<T>()`: Throw JavaScript exception
- `heap()`: Access GC heap

### 2. Bytecode Interpreter

**Location**: `Libraries/LibJS/Bytecode/Interpreter.h`

Executes bytecode instructions generated from AST.

```cpp
class Interpreter {
    // Run bytecode from entry point
    ThrowCompletionOr<Value> run(ExecutionContext&, Executable&);

    // Register access
    Value& accumulator();  // Special accumulator register
    Value& reg(Register);  // General purpose registers

    // Operand access
    Value get(Operand);
    void set(Operand, Value);
};
```

**Execution Flow**:
1. Fetch instruction at program counter
2. Decode operands
3. Execute operation
4. Update program counter
5. Handle exceptions
6. Repeat

### 3. Heap and Garbage Collection

**Location**: `Libraries/LibJS/Heap/` (uses LibGC)

LibJS uses LibGC's garbage collector:

- **Mark-and-Sweep**: Traces reachable objects
- **Conservative Stack Scanning**: Finds roots on C++ stack
- **NanBoxing**: Efficient value representation (64-bit)

```cpp
// All GC objects inherit from Cell
class Cell : public GC::Cell {
    // Override to mark references
    virtual void visit_edges(Visitor&);
};

// Allocate on GC heap
auto obj = heap.allocate<MyObject>(realm, args...);
```

**Value Representation (NanBoxing)**:
```
64-bit Value:
┌─────────────────────────────────────────────────────────────┐
│ Sign(1) │ Exponent(11) │ Mantissa(52)                        │
└─────────────────────────────────────────────────────────────┘

Primitives (non-pointer):
- undefined: Special tag
- null:      Special tag
- boolean:   Tag + bit
- int32:     Tag + 32-bit integer
- double:    IEEE 754 double (NaN patterns avoided)

Objects (pointer):
- object:    Pointer + tag bits
- string:    Pointer + tag bits
- bigint:    Pointer + tag bits
- symbol:    Pointer + tag bits
```

### 4. Runtime Objects

**Location**: `Libraries/LibJS/Runtime/`

**Object Hierarchy**:
```
Cell (GC base)
  └─ Object
      ├─ FunctionObject
      │   ├─ NativeFunction (C++ built-ins)
      │   └─ ECMAScriptFunctionObject (JS functions)
      ├─ Array
      ├─ StringObject
      ├─ NumberObject
      ├─ BooleanObject
      ├─ Date
      ├─ RegExpObject
      ├─ Error
      │   ├─ TypeError
      │   ├─ ReferenceError
      │   ├─ RangeError
      │   └─ ...
      └─ ... (many more)
```

**Key Runtime Types**:
- **Value**: NanBoxed union of all JS values
- **PrimitiveString**: Immutable string
- **BigInt**: Arbitrary precision integer
- **Symbol**: Unique identifier

### 5. Execution Context

**Location**: `Libraries/LibJS/Runtime/ExecutionContext.h`

Represents a function call frame:

```cpp
struct ExecutionContext {
    // Realm (global object + intrinsics)
    GC::Ptr<Realm> realm;

    // Lexical environment (variables)
    GC::Ptr<Environment> lexical_environment;

    // Variable environment
    GC::Ptr<Environment> variable_environment;

    // Private environment (for private class members)
    GC::Ptr<PrivateEnvironment> private_environment;

    // Function object (if function call)
    GC::Ptr<FunctionObject> function;

    // Bytecode (if executing bytecode)
    GC::Ptr<Bytecode::Executable> executable;

    // Registers for bytecode
    Vector<Value> registers_and_constants_and_locals_arguments;
};
```

### 6. Realm and Intrinsics

**Location**: `Libraries/LibJS/Runtime/Realm.h`

A Realm contains:
- Global object
- Global environment
- Built-in objects (intrinsics)

```cpp
class Realm : public Cell {
    // Global object (window in browsers, global in Node)
    GC::Ptr<GlobalObject> m_global_object;

    // Global environment
    GC::Ptr<GlobalEnvironment> m_global_environment;

    // Intrinsics (built-in prototypes and constructors)
    struct Intrinsics {
        GC::Ref<Object> object_prototype;
        GC::Ref<FunctionPrototype> function_prototype;
        GC::Ref<ArrayPrototype> array_prototype;
        GC::Ref<ObjectConstructor> object_constructor;
        // ... all built-ins
    };
};
```

### 7. Environments (Scopes)

**Location**: `Libraries/LibJS/Runtime/Environment.h`

Manages variable bindings:

```cpp
// Base class
class Environment : public Cell {
    virtual ThrowCompletionOr<bool> has_binding(FlyString const&) = 0;
    virtual ThrowCompletionOr<Value> get_binding_value(FlyString const&) = 0;
    virtual ThrowCompletionOr<void> set_mutable_binding(FlyString const&, Value) = 0;
};

// Declarative environment (let, const, function params)
class DeclarativeEnvironment : public Environment {
    HashMap<FlyString, Binding> m_bindings;
};

// Object environment (with statement, global)
class ObjectEnvironment : public Environment {
    GC::Ptr<Object> m_binding_object;
};

// Function environment (has 'this' binding)
class FunctionEnvironment : public DeclarativeEnvironment {
    Value m_this_value;
    GC::Ptr<FunctionObject> m_function_object;
};
```

## Bytecode System

### Instruction Format

```cpp
class Instruction {
    enum class Type {
        Add, Subtract, Multiply, Divide,
        GetById, SetById, GetByValue, SetByValue,
        Call, Construct, Return,
        Jump, JumpIf,
        LoadImmediate, Mov,
        // ... many more
    };

    Type type() const;
    void execute(Interpreter&) const;
};
```

### Register-Based Architecture

LibJS bytecode uses registers (not stack-based):

```
Registers:
- $acc:    Accumulator (special purpose)
- $0-$N:   General purpose registers
- @0-@N:   Constants
- local0-localN: Function locals
- arg0-argN: Function arguments
```

Example bytecode:
```
LoadImmediate $0, 42      ; $0 = 42
LoadImmediate $1, 10      ; $1 = 10
Add $2, $0, $1            ; $2 = $0 + $1
Return $2                 ; return $2
```

### Code Generation

**Location**: `Libraries/LibJS/Bytecode/ASTCodegen.cpp`

Each AST node implements `generate_bytecode()`:

```cpp
class BinaryExpression : public Expression {
    Bytecode::CodeGenerationErrorOr<Optional<ScopedOperand>>
    generate_bytecode(Bytecode::Generator&) const override;
};
```

## Optimization Techniques

### 1. Inline Caches

Property access uses inline caches:

```cpp
struct CacheableGetPropertyMetadata {
    enum class Type {
        NotCacheable,
        GetOwnProperty,
        GetPropertyInPrototypeChain,
    };
    Type type;
    Optional<u32> property_offset;
    GC::Ptr<Object const> prototype;
};
```

### 2. Shape Optimization

Objects with same properties share a "shape":

```cpp
class Shape : public Cell {
    // Property layout
    HashMap<StringOrSymbol, PropertyMetadata> m_properties;

    // Transition cache
    HashMap<StringOrSymbol, GC::Ptr<Shape>> m_transitions;
};
```

### 3. Fast Paths

Critical operations have fast paths:

```cpp
// Fast path for int32 addition
if (lhs.is_int32() && rhs.is_int32()) {
    return Value(lhs.as_i32() + rhs.as_i32());
}

// Slow path for generic addition
return TRY(add_impl(vm, lhs, rhs));
```

### 4. Bytecode Builtins

Common functions can be inlined by bytecode:

```cpp
define_native_function(realm, vm.names.abs, abs, 1, attr,
    Bytecode::Builtin::MathAbs);
```

## Error Handling

### Completion Types

```cpp
enum class CompletionType {
    Normal,    // Regular return
    Break,     // break statement
    Continue,  // continue statement
    Return,    // return statement
    Throw,     // exception thrown
};

// ThrowCompletionOr<T> - may throw or return T
template<typename T>
using ThrowCompletionOr = ErrorOr<T, Completion>;
```

### Exception Propagation

```cpp
// TRY macro propagates exceptions (like Rust's ?)
auto result = TRY(operation_that_can_throw(vm));

// Throw exception
return vm.throw_completion<TypeError>(ErrorType::NotAnObject);
```

## Performance Characteristics

**Fast Operations**:
- Property access with inline cache hit: O(1)
- Integer arithmetic: O(1)
- Array index access: O(1) for packed arrays

**Slow Operations**:
- Dynamic property addition: O(n) for shape transition
- Prototype chain lookup: O(d) where d = depth
- String concatenation: O(n) allocation

**GC Overhead**:
- Mark phase: O(live objects)
- Sweep phase: O(heap size)
- Typical pause: 1-10ms

## Integration with LibWeb

LibJS integrates with LibWeb for DOM APIs:

```cpp
// WebIDL bindings
class HTMLElement : public Element {
    JS_OBJECT(HTMLElement, Element);

    // Exposed to JavaScript
    String class_name() const;
    void set_class_name(String);

    // IDL attribute: element.className
    virtual String const& class_name() const;
};
```

**Bindings Generation**:
1. Write WebIDL file (e.g., HTMLElement.idl)
2. Run IDL compiler
3. Generates C++ wrapper classes
4. Exposes to JavaScript via Realm intrinsics

## Testing and Debugging

**Test Infrastructure**:
- `Tests/LibJS/`: JavaScript test suite
- `test-js`: Test runner
- Test262: ECMAScript conformance tests

**Debugging Tools**:
- `g_dump_bytecode`: Print generated bytecode
- VM execution traces
- GC statistics
- Heap snapshots

**Common Debug Points**:
```cpp
// Enable bytecode dump
bool g_dump_bytecode = true;

// Verify invariants
VERIFY(value.is_object());
VERIFY(!execution_context_stack.is_empty());

// Debug output
dbgln("Value: {}", value.to_string_without_side_effects());
```

## Further Reading

- ECMAScript Specification: https://tc39.es/ecma262/
- LibGC Documentation: `Libraries/LibGC/`
- Bytecode Reference: `Libraries/LibJS/Bytecode/`
- Runtime Reference: `Libraries/LibJS/Runtime/`
