/**
 * @file VM.cpp
 * @brief JavaScript VM stub.
 * @details Real bytecode compiler + interpreter in bead 7.
 * @copyright 2026, Nitrogen Browser Project
 */

#include "js/VM.h"

namespace js {

VM::VM() = default;
VM::~VM() = default;

Value VM::execute(const std::string& /*code*/) {
    return Value(); // placeholder – real interpreter in bead 7
}

} // namespace js