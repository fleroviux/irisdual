
#pragma once

#include <atom/integer.hpp>
#include <dual/arm/cpu.hpp>

#include "address.hpp"
#include "host_reg.hpp"

namespace dual::arm::jit::a64mir {

class Operand {
  public:
    enum class Type {
      Null,
      HostReg,
      Address
    };

    Operand() : m_type{Type::Null} {}
    explicit Operand(Address address) : m_type{Type::Address}, m_address{address} {}
    explicit Operand(HostReg host_reg) : m_type{Type::HostReg}, m_host_reg{host_reg} {}

    [[nodiscard]] Type GetType() const { return m_type; }
    [[nodiscard]] bool Is(Type type) const { return m_type == type; }

    [[nodiscard]] HostReg AsHostReg() const {
      DebugCheckType(Type::HostReg);
      return m_host_reg;
    }

    [[nodiscard]] const Address& AsAddress() const {
      DebugCheckType(Type::Address);
      return m_address;
    }

  private:
    void DebugCheckType(Type type) const {
#ifndef NDEBUG
      if(m_type != type) {
        ATOM_PANIC("bad operand type: expected {} but got {}", (int)type, (int)m_type);
      }
#endif
    }

    Type m_type;
    union {
      HostReg m_host_reg;
      Address m_address;
    };
};

} // namespace dual::arm::jit::a64mir
