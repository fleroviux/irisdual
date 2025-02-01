
#pragma once

#include <vector>

#include "basic_block.hpp"

namespace dual::arm::jit::ir {

struct Function {
  std::vector<BasicBlock> basic_blocks{};
};

} // namespace dual::arm::jit::ir
