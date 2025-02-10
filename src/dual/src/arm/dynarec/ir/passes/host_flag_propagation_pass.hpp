
#pragma once

#include <atom/integer.hpp>
#include <array>

#include "arm/dynarec/ir/pass.hpp"

namespace dual::arm::jit::ir {

class HostFlagPropagationPass final : public Pass {
  public:
    void Run(Function& function) override;

  private:
    using FlagState = std::array<Value::ID, 4>;

    static void RunBasicBlock(BasicBlock& basic_block);
};

} // namespace dual::arm::jit::ir
