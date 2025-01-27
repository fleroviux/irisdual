
#pragma once

#include "arm/dynarec/ir/function.hpp"

namespace dual::arm::jit {

class Backend {
  public:
    virtual ~Backend() = default;

    virtual void Execute(const ir::Function& function) = 0;
};

} // namespace dual::arm::jit
