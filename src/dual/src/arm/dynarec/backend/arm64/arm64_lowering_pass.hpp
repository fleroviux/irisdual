
#pragma once

#include <atom/arena.hpp>

#include "arm/dynarec/ir/basic_block.hpp"

namespace dual::arm::jit {

class ARM64LoweringPass {
  public:
    void Run(const ir::BasicBlock& basic_block, atom::Arena& memory_arena);

  private:
};

} // namespace dual::arm::jit