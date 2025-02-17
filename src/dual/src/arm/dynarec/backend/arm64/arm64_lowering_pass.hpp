
#pragma once

#include <atom/arena.hpp>
#include <optional>
#include <vector>

#include "arm/dynarec/ir/basic_block.hpp"
#include "mir/vreg.hpp"

namespace dual::arm::jit {

class ARM64LoweringPass {
  public:
    void Run(const ir::BasicBlock& basic_block, atom::Arena& memory_arena);

  private:
    std::vector<bool> m_value_has_vreg{}; // This is only really used for debugging...
    std::vector<a64mir::VReg::ID> m_value_to_vreg{};
};

} // namespace dual::arm::jit