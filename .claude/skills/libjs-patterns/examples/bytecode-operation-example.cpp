/*
 * Example: Adding a Bytecode Operation in LibJS
 *
 * This example demonstrates:
 * - Defining a new bytecode operation
 * - Implementing execution logic
 * - Generating bytecode from AST
 * - Integrating with the interpreter
 *
 * Example: Implementing a hypothetical "LogValue" operation
 * that logs a value to console for debugging
 */

// ===== Step 1: Define the operation in Bytecode/Op.h =====

#pragma once

#include <LibJS/Bytecode/Instruction.h>
#include <LibJS/Bytecode/Operand.h>

namespace JS::Bytecode::Op {

// Define the bytecode operation
class LogValue final : public Instruction {
public:
    LogValue(Operand src, IdentifierTableIndex label)
        : Instruction(Type::LogValue)
        , m_src(src)
        , m_label(label)
    {
    }

    // Execute the operation
    void execute_impl(Bytecode::Interpreter&) const;

    // Convert to string for debugging
    ByteString to_byte_string_impl(Bytecode::Executable const&) const;

    // Visit operands for analysis
    void visit_operands_impl(Function<void(Operand&)> visitor)
    {
        visitor(m_src);
    }

private:
    Operand m_src;                      // Source operand to log
    IdentifierTableIndex m_label;       // Optional label string
};

}

// ===== Step 2: Implement execution in Bytecode/Op.cpp =====

#include <LibJS/Bytecode/Interpreter.h>
#include <LibJS/Runtime/Value.h>
#include <AK/Format.h>

namespace JS::Bytecode::Op {

void LogValue::execute_impl(Bytecode::Interpreter& interpreter) const
{
    auto& vm = interpreter.vm();

    // Get the value from the operand
    auto value = interpreter.get(m_src);

    // Get the label string if provided
    auto label = interpreter.get_identifier(m_label);

    // Log to console
    if (label.has_value()) {
        outln("[DEBUG {}]: {}", label.value(), value.to_string_without_side_effects());
    } else {
        outln("[DEBUG]: {}", value.to_string_without_side_effects());
    }

    // No result value for this operation
    // (Some operations store results in accumulator or destination operand)
}

ByteString LogValue::to_byte_string_impl(Bytecode::Executable const& executable) const
{
    auto label = executable.get_identifier(m_label);
    if (label.has_value())
        return ByteString::formatted("LogValue {} (label: {})", m_src, label.value());
    return ByteString::formatted("LogValue {}", m_src);
}

}

// ===== Step 3: Register in instruction enumeration =====
// In Bytecode/Instruction.h, add to JS_ENUMERATE_BYTECODE_OPS macro:

/*
#define JS_ENUMERATE_BYTECODE_OPS(O) \
    O(Add)                           \
    O(BitwiseAnd)                    \
    O(Call)                          \
    O(CreateVariable)                \
    ... existing ops ...             \
    O(LogValue)                      \  // Add here
    O(Mov)                           \
    ... more ops ...
*/

// ===== Step 4: Generate bytecode from AST (in Bytecode/ASTCodegen.cpp) =====

#include <LibJS/AST.h>
#include <LibJS/Bytecode/Generator.h>
#include <LibJS/Bytecode/Op.h>

namespace JS {

// Example: Generate LogValue for a hypothetical AST node
Bytecode::CodeGenerationErrorOr<Optional<ScopedOperand>> LogExpression::generate_bytecode(Bytecode::Generator& generator) const
{
    // Generate bytecode for the value expression
    auto value_reg = TRY(m_value->generate_bytecode(generator)).value();

    // Get or create identifier for the label
    auto label_index = generator.intern_identifier(m_label);

    // Emit the LogValue instruction
    generator.emit<Bytecode::Op::LogValue>(value_reg, label_index);

    // Return the value (pass through)
    return value_reg;
}

}

// ===== Step 5: Example of more complex operations =====

namespace JS::Bytecode::Op {

// Example: BinaryOperation (Add, Subtract, etc.)
class Add final : public Instruction {
public:
    Add(Operand dst, Operand lhs, Operand rhs)
        : Instruction(Type::Add)
        , m_dst(dst)
        , m_lhs(lhs)
        , m_rhs(rhs)
    {
    }

    void execute_impl(Bytecode::Interpreter& interpreter) const
    {
        auto& vm = interpreter.vm();

        // Get operands
        auto lhs = interpreter.get(m_lhs);
        auto rhs = interpreter.get(m_rhs);

        // Perform addition (may throw)
        auto result = TRY_OR_SET_EXCEPTION(add(vm, lhs, rhs));

        // Store result in destination
        interpreter.set(m_dst, result);
    }

    ByteString to_byte_string_impl(Bytecode::Executable const&) const
    {
        return ByteString::formatted("Add {}, {}, {}", m_dst, m_lhs, m_rhs);
    }

    void visit_operands_impl(Function<void(Operand&)> visitor)
    {
        visitor(m_dst);
        visitor(m_lhs);
        visitor(m_rhs);
    }

private:
    Operand m_dst;
    Operand m_lhs;
    Operand m_rhs;
};

// Example: Jump operation
class Jump final : public Instruction {
public:
    explicit Jump(Label target)
        : Instruction(Type::Jump)
        , m_target(target)
    {
    }

    void execute_impl(Bytecode::Interpreter& interpreter) const
    {
        // Jumping is handled by the interpreter's dispatch loop
        // This just marks the target
    }

    Label target() const { return m_target; }

    ByteString to_byte_string_impl(Bytecode::Executable const&) const
    {
        return ByteString::formatted("Jump {}", m_target);
    }

    void visit_operands_impl(Function<void(Operand&)>) { }

private:
    Label m_target;
};

// Example: Conditional jump
class JumpIf final : public Instruction {
public:
    JumpIf(Operand condition, Label true_target, Label false_target)
        : Instruction(Type::JumpIf)
        , m_condition(condition)
        , m_true_target(true_target)
        , m_false_target(false_target)
    {
    }

    void execute_impl(Bytecode::Interpreter& interpreter) const
    {
        auto& vm = interpreter.vm();

        // Evaluate condition
        auto condition = interpreter.get(m_condition);
        auto boolean = TRY_OR_SET_EXCEPTION(condition.to_boolean());

        // Jump handled by interpreter based on boolean result
    }

    Label true_target() const { return m_true_target; }
    Label false_target() const { return m_false_target; }

    ByteString to_byte_string_impl(Bytecode::Executable const&) const
    {
        return ByteString::formatted("JumpIf {}, true:{}, false:{}",
            m_condition, m_true_target, m_false_target);
    }

    void visit_operands_impl(Function<void(Operand&)> visitor)
    {
        visitor(m_condition);
    }

private:
    Operand m_condition;
    Label m_true_target;
    Label m_false_target;
};

}

// ===== Step 6: Code generation patterns =====

namespace JS::Bytecode {

class GeneratorExamples {
public:
    // Pattern: Generating simple operation
    static void generate_simple_op(Generator& generator)
    {
        auto lhs = generator.allocate_register();
        auto rhs = generator.allocate_register();
        auto result = generator.allocate_register();

        // Emit instructions
        generator.emit<Op::LoadImmediate>(lhs, Value(42));
        generator.emit<Op::LoadImmediate>(rhs, Value(10));
        generator.emit<Op::Add>(result, lhs, rhs);
    }

    // Pattern: Generating control flow
    static void generate_if_statement(Generator& generator,
                                      ASTNode const& condition,
                                      ASTNode const& consequent,
                                      Optional<ASTNode> const& alternate)
    {
        // Generate condition
        auto condition_reg = generator.generate_bytecode(condition).value();

        // Create labels for branches
        auto consequent_label = generator.make_label();
        auto alternate_label = generator.make_label();
        auto end_label = generator.make_label();

        // Conditional jump
        generator.emit<Op::JumpIf>(condition_reg, consequent_label,
            alternate.has_value() ? alternate_label : end_label);

        // Consequent block
        generator.emit_label(consequent_label);
        generator.generate_bytecode(consequent);
        generator.emit<Op::Jump>(end_label);

        // Alternate block (if exists)
        if (alternate.has_value()) {
            generator.emit_label(alternate_label);
            generator.generate_bytecode(alternate.value());
        }

        // End
        generator.emit_label(end_label);
    }

    // Pattern: Generating loop
    static void generate_while_loop(Generator& generator,
                                    ASTNode const& condition,
                                    ASTNode const& body)
    {
        auto loop_start = generator.make_label();
        auto loop_end = generator.make_label();

        // Loop start - evaluate condition
        generator.emit_label(loop_start);
        auto condition_reg = generator.generate_bytecode(condition).value();

        // Jump to end if false
        generator.emit<Op::JumpIfFalse>(condition_reg, loop_end);

        // Loop body
        generator.generate_bytecode(body);

        // Jump back to start
        generator.emit<Op::Jump>(loop_start);

        // Loop end
        generator.emit_label(loop_end);
    }
};

}

/*
 * Key Bytecode Patterns:
 *
 * 1. Operation Structure:
 *    - Inherit from Instruction
 *    - Store operands as members
 *    - Implement execute_impl()
 *    - Implement to_byte_string_impl()
 *    - Implement visit_operands_impl()
 *
 * 2. Execution Patterns:
 *    - Get operands: interpreter.get(operand)
 *    - Set results: interpreter.set(operand, value)
 *    - Use accumulator: interpreter.accumulator()
 *    - Handle exceptions: TRY_OR_SET_EXCEPTION()
 *
 * 3. Code Generation:
 *    - Allocate registers: generator.allocate_register()
 *    - Emit instructions: generator.emit<Op::Name>(...)
 *    - Create labels: generator.make_label()
 *    - Emit labels: generator.emit_label(label)
 *
 * 4. Common Operation Types:
 *    - Arithmetic: Add, Subtract, Multiply, Divide
 *    - Logical: BitwiseAnd, BitwiseOr, Not
 *    - Comparison: LessThan, Equals, StrictEquals
 *    - Control flow: Jump, JumpIf, Return
 *    - Property access: GetById, SetById, GetByValue
 *    - Function calls: Call, Construct
 *    - Variable access: GetVariable, SetVariable
 *
 * 5. Operand Types:
 *    - Register: Temporary values
 *    - Constant: Compile-time constants
 *    - Local: Function locals
 *    - Argument: Function arguments
 *
 * 6. Testing:
 *    - Write JavaScript tests in Tests/LibJS/
 *    - Test bytecode generation
 *    - Test execution results
 *    - Test error cases
 */
