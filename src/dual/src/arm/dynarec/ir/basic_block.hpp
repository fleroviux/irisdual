
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

  ID id;
  Instruction* head{};
  Instruction* tail{};
  std::vector<Value*> values{};
};

} // namespace dual::arm::jit::ir
