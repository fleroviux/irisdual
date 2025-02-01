
#pragma once

#include <atom/non_copyable.hpp>
#include <algorithm>
#include <vector>

#include "instruction.hpp"
#include "value.hpp"

namespace dual::arm::jit::ir {

struct BasicBlock : atom::NonCopyable {
  Instruction* head{};
  Instruction* tail{};
  std::vector<Value*> values{};

  // Needed because the implicit move constructor is deleted due to atom::NonCopyable(?)
  BasicBlock() = default;
  BasicBlock(BasicBlock&& basic_block) {
    std::swap(head, basic_block.head);
    std::swap(tail, basic_block.tail);
    std::swap(values, basic_block.values);
  }
};

} // namespace dual::arm::jit::ir
