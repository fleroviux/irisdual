
#pragma once

#include <atom/integer.hpp>

namespace dual::arm::jit::a64mir {

class HostReg {
  public:
    enum class Type : u16 {
      WReg,
      XReg
    };

    constexpr HostReg(Type type, u16 reg_index) : m_type{type}, m_reg_index{reg_index} {}

    [[nodiscard]] constexpr Type GetType() const { return m_type; }
    [[nodiscard]] constexpr u16 Index() const { return m_reg_index; }

  private:
    Type m_type;
    u16 m_reg_index;
};

class WHostReg : public HostReg {
  public:
    constexpr explicit WHostReg(u16 reg_index) : HostReg{Type::WReg, reg_index} {}
};

class XHostReg : public HostReg {
  public:
    constexpr explicit XHostReg(u16 reg_index) : HostReg{Type::XReg, reg_index} {}
};

} // namespace dual::arm::jit::a64mir
