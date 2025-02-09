
#pragma once

#include "arm/dynarec/ir/pass.hpp"

namespace dual::arm::jit::ir {

class GuestStateAccessRemovalPass : public Pass {
  public:
    void Run(ir::Function& function) override;

  private:
    static void RunBasicBlock(ir::BasicBlock& basic_block);
};

} // namespace dual::arm::jit::ir
