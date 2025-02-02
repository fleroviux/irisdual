
#pragma once

#include <atom/integer.hpp>

namespace dual::arm::jit::a64mir {

struct VReg {
  using ID = u16;

  explicit VReg(ID id) : id{id} {}

  ID id;
};

} // namespace dual::arm::jit::a64mir