
#pragma once

#include <atom/integer.hpp>
#include <array>
#include <vector>

#include "arm/dynarec/ir/pass.hpp"

namespace dual::arm::jit::ir {

/**
 * This IR pass tracks where flags are originally created and if possible rewrites instructions
 * with host flag inputs to refer to the original host flags value where the relevant flags were created.
 * This makes it possible to for example trace flags going into a BR_IF instruction back to the host flags output of an
 * SUB.S, which originally computed the flags.
 *
 * This works even if the original host values value got converted to a NZCV value,
 * merged with another NZCV value (usually previous CPSR value) and then converted back to a host flags value.
 *
 * In the following IR code:
 * ```
 * v2, v3 := sub.s v0, v1
 * v4     := ldcpsr
 * v5     := cvt.hflag.nzcv v3
 * v6     := ldconst 0xF0000000
 * v7     := bitcmb v4, v5, v6
 *           stcpsr v7
 * v8     := cvt.nzcv.hflag v7
 *           br_if eq, v8, @bb_1, @bb_2
 * ```
 * the BR_IF instruction will be rewritten to use v3 instead of v8.
 * The CVT.NZCV.HFLAG instruction can later be removed by dead code removal.
 *
 * This pass helps not only with removing unnecessary host flags to NZCV conversions, but also with avoiding
 * unnecessary loads of host flag values into the host flags register when the original host flags value still is in that register.
 *
 * The pass requires Guest State Access Removal Pass to run first in order to function as expected.
 */

class HostFlagPropagationPass final : public Pass {
  public:
    void Run(Function& function) override;

  private:
    using FlagState = std::array<Value::ID, 4>;

    void RunBasicBlock(BasicBlock& basic_block);

    std::vector<FlagState> m_flag_state_map{};
};

} // namespace dual::arm::jit::ir
