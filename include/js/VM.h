#ifndef JS_VM_H
#define JS_VM_H

#include "Value.h"
#include "BytecodeCompiler.h"
#include "Heap.h"
#include "GC.h"
#include "Promise.h"
#include <stack>
#include <vector>
#include <unordered_map>
#include <functional>

namespace js {

struct CallFrame {
    BytecodeFunction* function;
    size_t pc = 0;
    size_t base = 0;
};

class VM {
public:
    VM();
    
    Value execute(const std::string& source);
    Value call(Function* func, const std::vector<Value>& args);
    
    Value getResult() const { return result_; }
    
    Heap& heap() { return heap_; }
    GC& gc() { return gc_; }
    
    void setCurrentContext(class JSContext* ctx) { context_ = ctx; }
    class JSContext* currentContext() const { return context_; }
    
private:
    std::stack<Value> operand_stack_;
    std::vector<Value> value_stack_;
    std::vector<CallFrame> call_frames_;
    BytecodeCompiler compiler_;
    Heap heap_;
    GC gc_;
    
    Value result_;
    
    JSContext* context_ = nullptr;
    
    void executeFunction(BytecodeFunction* func);
    void runInstruction(const Instruction& instr);
    Value popOperand();
    void pushOperand(const Value& val);
    
    Value resolveIdentifier(const std::string& name);
    void storeIdentifier(const std::string& name, const Value& val);
};

} // namespace js

#endif // JS_VM_H