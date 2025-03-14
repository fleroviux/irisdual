
#pragma once

#include <string>

#include "function.hpp"

namespace dual::arm::jit::ir {

std::string disassemble(const Function& function);
std::string disassemble(const BasicBlock& basic_block, const char* indent = nullptr);
std::string disassemble(const BasicBlock& basic_block, const Instruction& instruction);

} // namespace dual::arm::jit::ir