
#pragma once

#include "arm/dynarec/ir/pass.hpp"

namespace dual::arm::jit::ir {

class HostFlagPropagationPass final : public Pass {
  public:
    void Run(Function& function) override;

  private:
    struct FlagState {
      Value::ID n_flag{Value::invalid_id};
      Value::ID z_flag{Value::invalid_id};
      Value::ID c_flag{Value::invalid_id};
      Value::ID v_flag{Value::invalid_id};
    };

    static void RunBasicBlock(BasicBlock& basic_block);
};

} // namespace dual::arm::jit::ir
