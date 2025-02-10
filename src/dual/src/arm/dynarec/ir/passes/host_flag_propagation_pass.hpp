
#pragma once

#include <atom/integer.hpp>
#include <array>

#include "arm/dynarec/ir/pass.hpp"

namespace dual::arm::jit::ir {

/**
 * This IR pass tracks where flags are originally created and if possible rewrites instructions
 * with host flag inputs to refer to the original host flags value where the relevant flags were created.
 * This allows tracing the flags going into a conditional branch (BR_IF) back to the SUB.S instruction which created them,
 * even if the original host flags value was converted to NZCV, merged into old CPSR, written to CPSR, loaded back from
 * CPSR and then converted back to a host flags value:
 * ```
 * v2, v3 := sub.s v0, v1
 * v4     := ldcpsr
 * v5     := cvt.hflag.nzcv v3
 * v6     := ldconst 0xF0000000
 * v7     := bitcmb v4, v5, v6
 *           stcpsr v7
 * v8     := ldcpsr
 * v9     := cvt.nzcv.hflag v8
 *           br_if eq, v9, @bb_1, @bb_2
 * ```
 *
 * The pass requires Guest State Access Removal Pass to run first in order to function properly.
 */

class HostFlagPropagationPass final : public Pass {
  public:
    void Run(Function& function) override;

  private:
    using FlagState = std::array<Value::ID, 4>;

    static void RunBasicBlock(BasicBlock& basic_block);
};

} // namespace dual::arm::jit::ir
