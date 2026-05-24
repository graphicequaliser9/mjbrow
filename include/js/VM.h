/**
 * @file js/VM.h
 * @brief JavaScript VM scaffolding.
 * @details Value type, heap, GC, bytecode compiler, and bytecode interpreter.
 *          Promise + microtask-queue stubs also live here until a dedicated js/Promise.h
 *          is extracted in a later bead.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef JS_VM_H
#define JS_VM_H

#include <string>
#include <vector>
#include <cstdint>

// ---------- js/Value.h stub ----------
namespace js {

enum class ValueType { Undefined, Null, Bool, Number, String, Object };

class Value {
public:
    ValueType type;
    // tagged payload; for now just a placeholder union
    union { bool b; double d; const char* s; void* o; } payload;

    Value() : type(ValueType::Undefined), payload{} {}
};

// ---------- js/Heap.h stub ----------
class Heap {
public:
    void* allocate(size_t /*bytes*/) { return nullptr; }
    void free(void* /*ptr*/) {}
};

// ---------- js/GC.h stub ----------
class GC {
public:
    void collect() {}
};

// ---------- js/Parser.h stub ----------
class Parser {
public:
    Value parse(const std::string& /*source*/) { return Value(); }
};

// ---------- js/BytecodeCompiler.h stub ----------
class BytecodeCompiler {
public:
    std::vector<uint8_t> compile(const std::string& /*source*/) { return {}; }
};

// ---------- js/Promise.h stub ----------
class Promise {
public:
    enum State { Pending, Fulfilled, Rejected } state{Pending};
    Value value;
};

// ---------- js/VM.h ----------
class VM {
public:
    VM();
    ~VM();

    /// @brief Executes JavaScript code.
    /// @param code The JavaScript string to execute.
    /// @return The result of the execution.
    Value execute(const std::string& code);

private:
    Heap heap_;
    GC gc_;
};

} // namespace js

#endif // JS_VM_H