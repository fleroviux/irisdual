
#pragma once

#include <oaknut/oaknut.hpp>
#include <vector>

#include "arm/dynarec/ir/value.hpp"

namespace dual::arm::jit {

class ARM64ValueLocation {
  public:
    enum class Type {
      None,
      WReg
    };

    ARM64ValueLocation() : m_type{Type::None} {}
    explicit ARM64ValueLocation(oaknut::WReg wreg) : m_type{Type::WReg}, m_wreg{wreg} {}

    [[nodiscard]] Type GetType() const { return m_type; }
    [[nodiscard]] bool Is(Type type) const { return m_type == type; }

    [[nodiscard]] oaknut::WReg AsWReg() const {
      DebugCheckType(Type::WReg);
      return m_wreg;
    }

  private:
    void DebugCheckType(Type type) const {
#ifndef NDEBUG
      if(m_type != type) {
        ATOM_PANIC("bad location type: expected {} but got {}", (int)type, (int)m_type);
      }
#endif
    }

    Type m_type;
    union {
      oaknut::WReg m_wreg;
    };
};

} // namespace dual::arm::jit
