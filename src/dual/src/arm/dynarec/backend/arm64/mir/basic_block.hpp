
#pragma once

#include <atom/integer.hpp>
#include <atom/non_copyable.hpp>

namespace dual::arm::jit::a64mir {

struct Instruction;

struct BasicBlock : atom::NonCopyable {
  using ID = u16;

  explicit BasicBlock(ID id) : id{id} {}

  ID id;
  Instruction* head{};
  Instruction* tail{};
};

} // namespace dual::arm::jit::a64mir
