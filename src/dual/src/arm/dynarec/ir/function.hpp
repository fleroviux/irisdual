
#pragma once

#include <atom/non_copyable.hpp>
#include <vector>

#include "basic_block.hpp"

namespace dual::arm::jit::ir {

struct Function : atom::NonCopyable {
  std::vector<BasicBlock*> basic_blocks{};
};

} // namespace dual::arm::jit::ir
