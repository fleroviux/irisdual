
#pragma once

#include <atom/integer.hpp>
#include <atom/non_copyable.hpp>
#include <algorithm>
#include <vector>

namespace dual::arm::jit::ir {

struct Instruction;
struct Value;

struct BasicBlock : atom::NonCopyable {
  using ID = u16;

  explicit BasicBlock(ID id) : id{id} {}

  // Needed because the implicit move constructor is deleted due to atom::NonCopyable(?)
  BasicBlock(BasicBlock&& basic_block) {
    std::swap(id, basic_block.id);
    std::swap(head, basic_block.head);
    std::swap(tail, basic_block.tail);
    std::swap(values, basic_block.values);
  }

  ID id;
  Instruction* head{};
  Instruction* tail{};
  std::vector<Value*> values{};
};

} // namespace dual::arm::jit::ir
