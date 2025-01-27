
#pragma once

#include "basic_block.hpp"

namespace dual::arm::jit::ir {

struct Function {
  BasicBlock basic_block{}; // just deal with a single basic block for now
};

} // namespace dual::arm::jit::ir
