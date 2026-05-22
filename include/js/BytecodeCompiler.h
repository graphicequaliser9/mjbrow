#ifndef JS_BYTECODE_COMPILER_H
#define JS_BYTECODE_COMPILER_H

#include <cstdint>
#include <vector>
#include <string>

namespace js {

enum class OpCode : uint8_t {
    // Stack operations
    NOP,
    PUSH_UNDEFINED,
    PUSH_NULL,
    PUSH_BOOL,
    PUSH_NUMBER,
    PUSH_STRING,
    PUSH_OBJECT,
    POP,
    
    // Variable operations
    LOAD_NAME,
    STORE_NAME,
    LOAD_LOCAL,
    STORE_LOCAL,
    
    // Property access
    GET_PROPERTY,
    SET_PROPERTY,
    
    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    
    // Comparison
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    
    // Control flow
    JMP,
    JMP_TRUE,
    JMP_FALSE,
    
    // Function operations
    CALL,
    RETURN,
    CLOSURE,
    
    // Object operations
    NEW_OBJECT,
    NEW_ARRAY,
    
    // Type operations
    TYPEOF,
    INSTANCEOF,
    
    // Template literals
    TEMPLATE_LITERAL,
    
    // Destructuring
    DESTRUCT_ARRAY,
    DESTRUCT_OBJECT
};

struct Instruction {
    OpCode opcode;
    uint32_t operand = 0;
    std::string string_operand;
    
    Instruction(OpCode op) : opcode(op) {}
    Instruction(OpCode op, uint32_t val) : opcode(op), operand(val) {}
    Instruction(OpCode op, const std::string& val) : opcode(op), string_operand(val) {}
};

class BytecodeFunction {
public:
    std::vector<Instruction> code;
    uint32_t param_count = 0;
    uint32_t local_count = 0;
    std::string name;
    
    void emit(OpCode op) { code.emplace_back(op); }
    void emit(OpCode op, uint32_t operand) { code.emplace_back(op, operand); }
    void emit(OpCode op, const std::string& operand) { code.emplace_back(op, operand); }
};

class BytecodeCompiler {
public:
    BytecodeCompiler();
    
    BytecodeFunction* compile(const std::string& source);
    
private:
    void compileExpression(const std::string& expr, BytecodeFunction* func);
    
    uint32_t getNextLabel();
};

} // namespace js

#endif // JS_BYTECODE_COMPILER_H