#include "js/VM.h"
#include "js/Parser.h"
#include "js/JSContext.h"

namespace js {

VM::VM() {
    // Constructor - context set via setCurrentContext
}

Value VM::execute(const std::string& source) {
    Parser parser;
    auto ast = parser.parse(source);
    
    BytecodeCompiler compiler;
    auto func = compiler.compile(source);
    
    if (func) {
        executeFunction(func);
    }
    
    return result_;
}

Value VM::call(Function* func, const std::vector<Value>& args) {
    std::vector<Value> saved_stack = value_stack_;
    
    // Push arguments
    for (const auto& arg : args) {
        pushOperand(arg);
    }
    
    // Simple call execution
    if (func->isNative()) {
        result_ = func->call(args);
    }
    
    value_stack_ = saved_stack;
    return result_;
}

void VM::pushOperand(const Value& val) {
    operand_stack_.push(val);
}

Value VM::popOperand() {
    if (operand_stack_.empty()) {
        return Value::undefined();
    }
    Value val = operand_stack_.top();
    operand_stack_.pop();
    return val;
}

void VM::executeFunction(BytecodeFunction* func) {
    for (size_t i = 0; i < func->code.size(); i++) {
        const auto& instr = func->code[i];
        runInstruction(instr);
    }
}

void VM::runInstruction(const Instruction& instr) {
    switch (instr.opcode) {
        case OpCode::PUSH_UNDEFINED:
            pushOperand(Value::undefined());
            break;
        case OpCode::PUSH_NULL:
            pushOperand(Value::null());
            break;
        case OpCode::PUSH_NUMBER:
            pushOperand(Value(instr.operand));
            break;
        case OpCode::PUSH_STRING:
            pushOperand(Value(instr.string_operand));
            break;
        case OpCode::POP:
            popOperand();
            break;
        case OpCode::ADD: {
            Value b = popOperand();
            Value a = popOperand();
            pushOperand(Value(a.asNumber() + b.asNumber()));
            break;
        }
        case OpCode::SUB: {
            Value b = popOperand();
            Value a = popOperand();
            pushOperand(Value(a.asNumber() - b.asNumber()));
            break;
        }
        case OpCode::MUL: {
            Value b = popOperand();
            Value a = popOperand();
            pushOperand(Value(a.asNumber() * b.asNumber()));
            break;
        }
        case OpCode::DIV: {
            Value b = popOperand();
            Value a = popOperand();
            pushOperand(Value(a.asNumber() / b.asNumber()));
            break;
        }
        case OpCode::RETURN:
            if (!operand_stack_.empty()) {
                result_ = operand_stack_.top();
            }
            break;
        default:
            break;
    }
}

Value VM::resolveIdentifier(const std::string& name) {
    if (context_) {
        return context_->global()->get(name);
    }
    return Value::undefined();
}

void VM::storeIdentifier(const std::string& name, const Value& val) {
    if (context_) {
        context_->global()->set(name, val);
    }
}

} // namespace js