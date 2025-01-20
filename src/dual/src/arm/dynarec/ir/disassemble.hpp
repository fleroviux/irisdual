
#pragma once

#include <string>

#include "basic_block.hpp"

namespace dual::arm::jit::ir {

std::string disassemble(const BasicBlock& basic_block);

} // namespace dual::arm::jit::ir