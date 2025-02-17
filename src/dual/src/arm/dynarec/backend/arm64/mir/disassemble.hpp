
#pragma once

#include <string>

#include "basic_block.hpp"

namespace dual::arm::jit::a64mir {

std::string disassemble(const BasicBlock& basic_block, const char* indent = nullptr);
std::string disassemble(const Instruction& instruction);

} // namespace dual::arm::jit::a64mir
