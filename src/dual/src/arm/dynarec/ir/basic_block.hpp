
#pragma once

#include <vector>

#include "instruction.hpp"
#include "value.hpp"

namespace dual::arm::jit::ir {

struct BasicBlock {
  Instruction* head{};
  Instruction* tail{};
  std::vector<Value*> values{};
};

} // namespace dual::arm::jit::ir
