
#pragma once

#include "arm/dynarec/ir/pass.hpp"

namespace dual::arm::jit::ir {

class DeadCodeRemovalPass : public Pass {
  public:
    void Run(Function& function) override;

  private:
    static void RunBasicBlock(BasicBlock& basic_block);
};

} // namespace dual::arm::jit::ir